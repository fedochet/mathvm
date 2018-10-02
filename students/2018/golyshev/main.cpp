#include <mathvm.h>
#include <parser.h>
#include <visitors.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <memory>
#include <streambuf>
#include <string>

using namespace mathvm;
using namespace std;
using namespace std::literals::string_literals;

ostream& operator<<(ostream& o, VarType type) {
    switch (type) {
        case VT_INVALID:
            o << "invalid";
            break;
        case VT_VOID:
            o << "void";
            break;
        case VT_DOUBLE:
            o << "double";
            break;
        case VT_INT:
            o << "int";
            break;
        case VT_STRING:
            o << "string";
            break;
    }

    return o;
}

ostream& operator<<(ostream& o, TokenKind kind) {
    #define PRINT_OP(t, s, p) \
        case t: o << s; \
        break;

    switch (kind) {
        case tTokenCount: break;
        FOR_TOKENS(PRINT_OP)
    }
    
    #undef PRINT_OP
    
    return o;
}

string escape(const string& s) {
    stringstream ss;
    for (char c : s) {
        switch (c) {
            case '\n':
                ss << "\\n";
                break;
            case '\t':
                ss << "\\t";
                break;
            case '\r':
                ss << "\\r";
                break;
            case '\\':
                ss << "\\\\";
                break;
            case '\'':
                ss << "\\'";
                break;
            default: 
                ss << c;
        }
    }

    return ss.str();
}

string loadFile(const string& filename) {
    ifstream file{filename};
    if (!file) {
        cerr << "Cannot read file: " << filename << endl;
        exit(1);
    }
    return {istreambuf_iterator<char>(file), istreambuf_iterator<char>()};
}

struct PrintVisitor : public AstBaseVisitor {
    PrintVisitor(ostream& buffer, size_t indentSize = 4): 
        _buffer(buffer), _indentSize(indentSize) {}
    
    virtual ~PrintVisitor() {}

    void visitBinaryOpNode(BinaryOpNode* node) override {
        _buffer << "(";
        node->left()->visit(this);
        _buffer << node->kind();
        node->right()->visit(this);
        _buffer << ")";
    }

    void visitUnaryOpNode(UnaryOpNode* node) override {
        _buffer << "(";
        _buffer << node->kind();
        node->operand()->visit(this);
        _buffer << ")";
    }

    void visitBlockNode(BlockNode* node) override {
        Scope::VarIterator varIterator(node->scope());
        while (varIterator.hasNext()) {
            indent();
            AstVar* var = varIterator.next();
            _buffer << var->type() 
                    << " "
                    << var->name()
                    << ";"
                    << endl;
        }

        for (size_t i = 0; i < node->nodes(); i++) {
            indent();
            AstNode* currNode = node->nodeAt(i);
            currNode->visit(this);
            if (!(currNode->isForNode() || currNode->isWhileNode() || currNode->isIfNode())) {
                _buffer << ";";
            }
            _buffer << endl;
        }
    }

    void visitStoreNode(StoreNode* node) override {
        _buffer << node->var()->name() 
                << ' ' 
                << node->op() 
                << ' ';
        node->value()->visit(this);
    }

    void visitStringLiteralNode(StringLiteralNode* node) override {
        _buffer << '"' << escape(node->literal()) << '"';
    }

    void visitDoubleLiteralNode(DoubleLiteralNode* node) override {
        _buffer << node->literal();
    }

    void visitIntLiteralNode(IntLiteralNode* node) override {
        _buffer << node->literal();
    }

    void visitLoadNode(LoadNode* node) override {
        _buffer << node->var()->name();
    }

    void visitForNode(ForNode* node) override {
        _buffer << "for (" << node->var()->name() << " in ";
        node->inExpr()->visit(this);
        _buffer << ") {" << endl; 
        
        increaseIndent();
        node->body()->visit(this);
        decreaseIndent();
        
        indent();
        _buffer << "}" << endl;
    }

    void visitWhileNode(WhileNode* node) override {
        _buffer << "while (";
        node->whileExpr()->visit(this);
        _buffer << ") {" << endl; 
        
        increaseIndent();
        node->loopBlock()->visit(this);
        decreaseIndent();
        
        indent();
        _buffer << "}" << endl;
    }

    void visitIfNode(IfNode* node) override {
        indent();
        _buffer << "if (";
        node->ifExpr()->visit(this);
        _buffer << ") {" << endl; 
        
        increaseIndent();
        node->thenBlock()->visit(this);
        decreaseIndent();
        
        indent();
        _buffer << "}";
        if (node->elseBlock()) {
            _buffer << " else {" << endl;;
            
            increaseIndent();
            node->elseBlock()->visit(this);
            decreaseIndent();
            
            indent();
            _buffer << "}";
        }

    }

    void visitCallNode(CallNode* node) {
        _buffer << node->name() << "(";

        bool first = true;
        for (size_t i = 0; i < node->parametersNumber(); i++) {
            if (!first) {
                _buffer << ", ";
            }
            
            node->parameterAt(i)->visit(this);
            first = false;
        }

        cout << ")";
    }

    void visitPrintNode(PrintNode* node) {
        _buffer << "print(";

        bool first = true;
        for (size_t i = 0; i < node->operands(); i++) {
            if (!first) {
                _buffer << ", ";
            }
            
            node->operandAt(i)->visit(this);
            first = false;
        }

        cout << ")";
    }

private:
    ostream& _buffer;
    const size_t _indentSize;
    size_t _indent { 0 };

    void indent() {
        _buffer << string(_indent, ' ');
    }

    void increaseIndent() {
        _indent += _indentSize;
    };

    void decreaseIndent() {
        _indent -= _indentSize;
    };
};

int main(int argc, char** argv) {
    string impl;
    string script_file;

    if (argc == 3) {
        if (argv[1] == "-p"s) {
            impl = "printer";
        } else if (argv[1] == "-i"s) {
            impl = "interpreter";
        } else if (argv[1] == "-j"s) {
            impl = "jit";
        } else {
            cerr << "Invalid option: " << argv[1] << endl;
            return 1;
        }
        script_file = argv[2];
    } else if (argc == 2) {
        impl = "interpreter";
        script_file = argv[1];
    } else {
        cerr << "Usage: mvm [OPTION] FILE" << endl;
        return 1;
    }

    string program = loadFile(script_file);

    unique_ptr<Translator> translator{Translator::create(impl)};

    if (impl == "printer") {
        Parser parser;
        unique_ptr<Status> parseStatus { parser.parseProgram(program) };

        if (parseStatus->isOk()) {
            PrintVisitor visitor(cout);
            parser.top()->node()->visit(&visitor);
        } else {
            uint32_t position = parseStatus->getPosition();
            uint32_t line = 0, offset = 0;
            positionToLineOffset(program, position, line, offset);
            cout << "Cannot parse programm: "
                << "expression at " << line << "," << offset << "; "
                << "error '" << parseStatus->getErrorCstr() << "'" << endl;
            return 1;
        }

        return 0;
    }

    if (!translator) {
        cout << "TODO: Implement translator factory in translator.cpp" << endl;
        return 1;
    }

    Code* code = 0;
    unique_ptr<Status> translateStatus{translator->translate(program, &code)};

    if (translateStatus->isError()) {
        uint32_t position = translateStatus->getPosition();
        uint32_t line = 0, offset = 0;
        positionToLineOffset(program, position, line, offset);
        cout << "Cannot translate expression: "
             << "expression at " << line << "," << offset << "; "
             << "error '" << translateStatus->getErrorCstr() << "'" << endl;
        return 1;
    }

    if (impl != "printer") {
        assert(code);

        vector<Var*> vars;
        unique_ptr<Status> execStatus{code->execute(vars)};

        if (execStatus->isError()) {
            cerr << "Cannot execute expression, error: " << execStatus->getErrorCstr();
        }

        delete code;
    }

    return 0;
}
