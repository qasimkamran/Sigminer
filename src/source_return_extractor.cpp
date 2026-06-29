#include "internal/source_return_extractor.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/Decl.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Stmt.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Error.h>

namespace source_return_extractor {

namespace {

std::string CompilationDatabaseDirectory(const std::string& compileCommandsPathOrDirectory)
{
    if (llvm::sys::path::filename(compileCommandsPathOrDirectory) == "compile_commands.json") {
        const llvm::StringRef parentPath = llvm::sys::path::parent_path(compileCommandsPathOrDirectory);
        if (parentPath.empty())
            return ".";
        return parentPath.str();
    }

    return compileCommandsPathOrDirectory;
}

std::string NormalizePath(llvm::StringRef path, llvm::StringRef workingDirectory)
{
    llvm::SmallString<256> normalizedPath(path);
    if (llvm::sys::path::is_relative(normalizedPath)) {
        llvm::SmallString<256> absolutePath(workingDirectory);
        llvm::sys::path::append(absolutePath, normalizedPath);
        normalizedPath = absolutePath;
    }

    llvm::sys::path::remove_dots(normalizedPath, true);
    return normalizedPath.str().str();
}

bool IsSourcePathArgument(
        const std::string& arg,
        llvm::StringRef filename,
        llvm::StringRef workingDirectory)
{
    return NormalizePath(arg, workingDirectory) ==
           NormalizePath(filename, workingDirectory);
}

const char* LanguageForSourceFile(llvm::StringRef filename)
{
    const llvm::StringRef extension = llvm::sys::path::extension(filename);
    if (extension == ".c")
        return "c";
    if (extension == ".cc" || extension == ".cpp" || extension == ".cxx" || extension == ".C")
        return "c++";
    return nullptr;
}

clang::tooling::ArgumentsAdjuster GetMissingInputFileAdjuster(std::string workingDirectory)
{
    return [workingDirectory = std::move(workingDirectory)](
                   const clang::tooling::CommandLineArguments& args,
                   llvm::StringRef filename) {
        clang::tooling::CommandLineArguments adjusted = args;
        const bool hasInputFile = std::any_of(
                adjusted.begin(),
                adjusted.end(),
                [filename, &workingDirectory](const std::string& arg) {
                    return IsSourcePathArgument(arg, filename, workingDirectory);
                });
        if (hasInputFile)
            return adjusted;

        if (const char* language = LanguageForSourceFile(filename)) {
            adjusted.emplace_back("-x");
            adjusted.emplace_back(language);
        }

        adjusted.emplace_back("-c");
        adjusted.emplace_back(filename.str());
        return adjusted;
    };
}

clang::tooling::ArgumentsAdjuster GetExtraClangArgsAdjuster(
        std::vector<std::string> extraClangArgs)
{
    return [extraClangArgs = std::move(extraClangArgs)](
                   const clang::tooling::CommandLineArguments& args,
                   llvm::StringRef) {
        clang::tooling::CommandLineArguments adjusted = args;
        adjusted.insert(adjusted.end(), extraClangArgs.begin(), extraClangArgs.end());
        return adjusted;
    };
}

sigminer::SourceLocation ToSourceLocation(
        const clang::SourceManager& sourceManager,
        clang::SourceLocation location)
{
    const clang::PresumedLoc presumedLocation = sourceManager.getPresumedLoc(location);
    if (presumedLocation.isInvalid())
        return {};

    return sigminer::SourceLocation{
            .file = presumedLocation.getFilename(),
            .line = presumedLocation.getLine(),
            .column = presumedLocation.getColumn(),
    };
}

struct ExtractionState
{
    std::vector<sigminer::SourceReturnCandidate>& sourceReturns;
    std::size_t matchingFunctionCount = 0;
};

class ReturnStmtVisitor : public clang::RecursiveASTVisitor<ReturnStmtVisitor>
{
public:
    ReturnStmtVisitor(
            const std::string& functionName,
            ExtractionState& state)
        : functionName(functionName),
          state(state)
    {
    }

    bool TraverseFunctionDecl(clang::FunctionDecl* functionDecl)
    {
        if (!functionDecl || !functionDecl->hasBody())
            return true;

        if (functionDecl->getQualifiedNameAsString() != functionName)
            return true;

        ++state.matchingFunctionCount;
        const clang::FunctionDecl* previousFunction = currentFunction;
        currentFunction = functionDecl;
        const bool traversalSucceeded =
                RecursiveASTVisitor<ReturnStmtVisitor>::TraverseStmt(functionDecl->getBody());
        currentFunction = previousFunction;
        return traversalSucceeded;
    }

    bool VisitReturnStmt(clang::ReturnStmt* returnStmt)
    {
        if (!currentFunction || !returnStmt)
            return true;

        const clang::SourceManager& sourceManager =
                currentFunction->getASTContext().getSourceManager();
        const clang::SourceLocation returnLocation = returnStmt->getReturnLoc();
        const clang::SourceLocation spellingLocation =
                sourceManager.getSpellingLoc(returnLocation);
        const clang::SourceLocation expansionLocation =
                sourceManager.getExpansionLoc(returnLocation);

        state.sourceReturns.push_back(sigminer::SourceReturnCandidate{
                .functionName = currentFunction->getQualifiedNameAsString(),
                .spellingLocation = ToSourceLocation(sourceManager, spellingLocation),
                .expansionLocation = ToSourceLocation(sourceManager, expansionLocation),
        });

        return true;
    }

private:
    std::string functionName{};
    ExtractionState& state;
    const clang::FunctionDecl* currentFunction = nullptr;
};

class ReturnStmtAstConsumer : public clang::ASTConsumer
{
public:
    ReturnStmtAstConsumer(
            const std::string& functionName,
            ExtractionState& state)
        : visitor(functionName, state)
    {
    }

    void HandleTranslationUnit(clang::ASTContext& context) override
    {
        visitor.TraverseDecl(context.getTranslationUnitDecl());
    }

private:
    ReturnStmtVisitor visitor;
};

class ReturnStmtFrontendAction : public clang::ASTFrontendAction
{
public:
    ReturnStmtFrontendAction(
            const std::string& functionName,
            ExtractionState& state)
        : functionName(functionName),
          state(state)
    {
    }

    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
            clang::CompilerInstance&,
            llvm::StringRef) override
    {
        return std::make_unique<ReturnStmtAstConsumer>(functionName, state);
    }

private:
    std::string functionName{};
    ExtractionState& state;
};

class ReturnStmtFrontendActionFactory : public clang::tooling::FrontendActionFactory
{
public:
    ReturnStmtFrontendActionFactory(
            const std::string& functionName,
            ExtractionState& state)
        : functionName(functionName),
          state(state)
    {
    }

    std::unique_ptr<clang::FrontendAction> create() override
    {
        return std::make_unique<ReturnStmtFrontendAction>(functionName, state);
    }

private:
    std::string functionName{};
    ExtractionState& state;
};

} // namespace

llvm::Expected<std::vector<sigminer::SourceReturnCandidate>> ExtractSourceReturnCandidates(
        const std::string& compileCommandsPathOrDirectory,
        const std::string& sourceFilePath,
        const std::string& functionName,
        const std::vector<std::string>& extraClangArgs)
{
    if (compileCommandsPathOrDirectory.empty() || sourceFilePath.empty() || functionName.empty()) {
        return llvm::createStringError(
                std::errc::invalid_argument,
                "compile commands path, source file path, and qualified function name are required");
    }

    std::vector<sigminer::SourceReturnCandidate> sourceReturns{};

    std::string error{};
    std::unique_ptr<clang::tooling::CompilationDatabase> compilations =
            clang::tooling::CompilationDatabase::loadFromDirectory(
                    CompilationDatabaseDirectory(compileCommandsPathOrDirectory),
                    error);
    if (!compilations) {
        return llvm::createStringError(
                std::errc::invalid_argument,
                "failed to load compilation database: %s",
                error.c_str());
    }

    const std::vector<clang::tooling::CompileCommand> compileCommands =
            compilations->getCompileCommands(sourceFilePath);
    if (compileCommands.empty()) {
        return llvm::createStringError(
                std::errc::no_such_file_or_directory,
                "no compile command found for source file: %s",
                sourceFilePath.c_str());
    }

    clang::tooling::ClangTool tool(*compilations, llvm::ArrayRef<std::string>{sourceFilePath});
    if (!extraClangArgs.empty())
        tool.appendArgumentsAdjuster(GetExtraClangArgsAdjuster(extraClangArgs));
    tool.appendArgumentsAdjuster(
            GetMissingInputFileAdjuster(compileCommands.front().Directory));
    ExtractionState state{.sourceReturns = sourceReturns};
    ReturnStmtFrontendActionFactory factory(functionName, state);
    if (tool.run(&factory) != 0) {
        return llvm::createStringError(
                std::errc::io_error,
                "Clang failed while extracting returns from: %s",
                sourceFilePath.c_str());
    }

    if (state.matchingFunctionCount == 0) {
        return llvm::createStringError(
                std::errc::no_such_file_or_directory,
                "qualified function not found: %s",
                functionName.c_str());
    }

    if (state.matchingFunctionCount > 1) {
        return llvm::createStringError(
                std::errc::invalid_argument,
                "qualified function name is ambiguous: %s",
                functionName.c_str());
    }

    return sourceReturns;
}

} // namespace source_return_extractor
