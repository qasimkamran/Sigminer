#include "internal/source_return_extractor.h"

#include <memory>
#include <string>
#include <vector>

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/Decl.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/Stmt.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/Support/Path.h>

namespace {

std::string CompilationDatabaseDirectory(const std::string& compileCommandsPathOrDirectory)
{
    if (llvm::sys::path::filename(compileCommandsPathOrDirectory) == "compile_commands.json")
        return llvm::sys::path::parent_path(compileCommandsPathOrDirectory).str();

    return compileCommandsPathOrDirectory;
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

class ReturnStmtVisitor : public clang::RecursiveASTVisitor<ReturnStmtVisitor>
{
public:
    ReturnStmtVisitor(
            const std::string& functionName,
            std::vector<sigminer::SourceReturnCandidate>& sourceReturns)
        : functionName(functionName),
          sourceReturns(sourceReturns)
    {
    }

    bool TraverseFunctionDecl(clang::FunctionDecl* functionDecl)
    {
        if (!functionDecl || !functionDecl->hasBody())
            return true;

        const bool wasInsideTargetFunction = insideTargetFunction;
        const clang::FunctionDecl* previousFunction = currentFunction;
        if (functionDecl->getNameAsString() == functionName) {
            insideTargetFunction = true;
            currentFunction = functionDecl;
            RecursiveASTVisitor<ReturnStmtVisitor>::TraverseStmt(functionDecl->getBody());
            currentFunction = previousFunction;
            insideTargetFunction = wasInsideTargetFunction;
            return true;
        }

        return true;
    }

    bool VisitReturnStmt(clang::ReturnStmt* returnStmt)
    {
        if (!insideTargetFunction || !currentFunction || !returnStmt)
            return true;

        const clang::SourceManager& sourceManager =
                currentFunction->getASTContext().getSourceManager();
        const clang::SourceLocation returnLocation = returnStmt->getReturnLoc();
        const clang::SourceLocation spellingLocation =
                sourceManager.getSpellingLoc(returnLocation);
        const clang::SourceLocation expansionLocation =
                sourceManager.getExpansionLoc(returnLocation);

        sourceReturns.push_back(sigminer::SourceReturnCandidate{
                .functionName = currentFunction->getNameAsString(),
                .spellingLocation = ToSourceLocation(sourceManager, spellingLocation),
                .expansionLocation = ToSourceLocation(sourceManager, expansionLocation),
        });

        return true;
    }

private:
    std::string functionName{};
    std::vector<sigminer::SourceReturnCandidate>& sourceReturns;
    bool insideTargetFunction = false;
    const clang::FunctionDecl* currentFunction = nullptr;
};

class ReturnStmtAstConsumer : public clang::ASTConsumer
{
public:
    ReturnStmtAstConsumer(
            const std::string& functionName,
            std::vector<sigminer::SourceReturnCandidate>& sourceReturns)
        : visitor(functionName, sourceReturns)
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
            std::vector<sigminer::SourceReturnCandidate>& sourceReturns)
        : functionName(functionName),
          sourceReturns(sourceReturns)
    {
    }

    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
            clang::CompilerInstance&,
            llvm::StringRef) override
    {
        return std::make_unique<ReturnStmtAstConsumer>(functionName, sourceReturns);
    }

private:
    std::string functionName{};
    std::vector<sigminer::SourceReturnCandidate>& sourceReturns;
};

class ReturnStmtFrontendActionFactory : public clang::tooling::FrontendActionFactory
{
public:
    ReturnStmtFrontendActionFactory(
            const std::string& functionName,
            std::vector<sigminer::SourceReturnCandidate>& sourceReturns)
        : functionName(functionName),
          sourceReturns(sourceReturns)
    {
    }

    std::unique_ptr<clang::FrontendAction> create() override
    {
        return std::make_unique<ReturnStmtFrontendAction>(functionName, sourceReturns);
    }

private:
    std::string functionName{};
    std::vector<sigminer::SourceReturnCandidate>& sourceReturns;
};

} // namespace

std::vector<sigminer::SourceReturnCandidate> ExtractSourceReturnCandidates(
        const std::string& compileCommandsPathOrDirectory,
        const std::string& sourceFilePath,
        const std::string& functionName)
{
    std::vector<sigminer::SourceReturnCandidate> sourceReturns{};
    if (compileCommandsPathOrDirectory.empty() || sourceFilePath.empty() || functionName.empty())
        return sourceReturns;

    std::string error{};
    std::unique_ptr<clang::tooling::CompilationDatabase> compilations =
            clang::tooling::CompilationDatabase::loadFromDirectory(
                    CompilationDatabaseDirectory(compileCommandsPathOrDirectory),
                    error);
    if (!compilations)
        return sourceReturns;

    clang::tooling::ClangTool tool(*compilations, llvm::ArrayRef<std::string>{sourceFilePath});
    ReturnStmtFrontendActionFactory factory(functionName, sourceReturns);
    if (tool.run(&factory) != 0)
        return {};

    return sourceReturns;
}
