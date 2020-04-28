
#if defined(_WIN32) || defined(_WIN64)
#include <malloc.h>
#else
#include <alloca.h>
#endif
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "compiler/ast.h"
#include "compiler/globallimits.h"
#include "compiler/lexer.h"
#include "compiler/operator.h"


void ast_ClearFunctionArgs(h64funcargs *fargs) {
    int i = 0;
    while (i < fargs->arg_count) {
        if (fargs->arg_name[i])
            free(fargs->arg_name[i]);
        if (fargs->arg_value[i])
            ast_FreeExpression(fargs->arg_value[i]);
        i++;
    }
    free(fargs->arg_name);
    fargs->arg_name = NULL;
    free(fargs->arg_value);
    fargs->arg_value = NULL;
    fargs->arg_count = 0;
}

int ast_VisitExpression(
        h64expression *expr, h64expression *parent,
        int (*visit_in)(
            h64expression *expr, h64expression *parent, void *ud
        ),
        int (*visit_out)(
            h64expression *expr, h64expression *parent, void *ud
        ),
        void *ud
        ) {
    if (!expr)
        return 1;

    if (visit_in) {
        if (!visit_in(expr, parent, ud))
            return 0;
    }

    int i = 0;
    switch (expr->type) {
    case H64EXPRTYPE_INVALID:
        // nothing to do;
        break;
    case H64EXPRTYPE_VARDEF_STMT:
        if (expr->vardef.value)
            if (!ast_VisitExpression(
                    expr->vardef.value, expr, visit_in, visit_out, ud
                    ))
                return 0;
        break;
    case H64EXPRTYPE_FUNCDEF_STMT:
    case H64EXPRTYPE_INLINEFUNC:
        i = 0;
        while (i < expr->funcdef.arguments.arg_count) {
            if (!ast_VisitExpression(
                    expr->funcdef.arguments.arg_value[i], expr,
                    visit_in, visit_out, ud
                    ))
                return 0;
            i++;
        }
        i = 0;
        while (i < expr->funcdef.stmt_count) {
            if (!ast_VisitExpression(
                    expr->funcdef.stmt[i], expr, visit_in, visit_out, ud
                    ))
                return 0;
            i++;
        }
        break;
    case H64EXPRTYPE_CALL_STMT:
        if (expr->callstmt.call)
            if (!ast_VisitExpression(
                    expr->callstmt.call, expr, visit_in, visit_out, ud
                    ))
                return 0;
        break;
    case H64EXPRTYPE_CLASSDEF_STMT:
        if (!ast_VisitExpression(
                expr->classdef.baseclass_ref, expr, visit_in, visit_out, ud
                ))
            return 0;
        i = 0;
        while (i < expr->classdef.vardef_count) {
            if (!ast_VisitExpression(
                    expr->classdef.vardef[i], expr, visit_in, visit_out, ud
                    ))
                return 0;
            i++;
        }
        i = 0;
        while (i < expr->classdef.funcdef_count) {
            if (!ast_VisitExpression(
                    expr->classdef.funcdef[i], expr, visit_in, visit_out, ud
                    ))
                return 0;
            i++;
        }
        break;
    case H64EXPRTYPE_IF_STMT: ;
        struct h64ifstmt *current_clause = &expr->ifstmt;
        while (current_clause) {
            struct h64ifstmt *next_clause = (
                current_clause->followup_clause
            );

            if (!ast_VisitExpression(
                    current_clause->conditional, expr, visit_in, visit_out, ud
                    ))
                return 0;
            i = 0;
            while (i < current_clause->stmt_count) {
                if (!ast_VisitExpression(
                        current_clause->stmt[i], expr, visit_in, visit_out, ud
                        ))
                    return 0;
                i++;
            }
            free(current_clause->stmt);
            current_clause = next_clause;
        }
        break;
    case H64EXPRTYPE_WHILE_STMT:
        if (!ast_VisitExpression(
                expr->whilestmt.conditional, expr, visit_in, visit_out, ud
                ))
            return 0;
        i = 0;
        while (i < expr->whilestmt.stmt_count) {
            if (!ast_VisitExpression(
                    expr->whilestmt.stmt[i], expr, visit_in, visit_out, ud
                    ))
                return 0;
            i++;
        }
        break;
    case H64EXPRTYPE_FOR_STMT:
        if (!ast_VisitExpression(
                expr->forstmt.iterated_container, expr,
                visit_in, visit_out, ud
                ))
            return 0;
        i = 0;
        while (i < expr->forstmt.stmt_count) {
            if (!ast_VisitExpression(
                    expr->forstmt.stmt[i], expr, visit_in, visit_out, ud
                    ))
                return 0;
            i++;
        }
        break;
    case H64EXPRTYPE_IMPORT_STMT:
        break;
    case H64EXPRTYPE_RETURN_STMT:
        if (!ast_VisitExpression(
                expr->returnstmt.returned_expression, expr,
                visit_in, visit_out, ud
                ))
            return 0;
        break;
    case H64EXPRTYPE_TRY_STMT:
        i = 0;
        while (i < expr->trystmt.trystmt_count) {
            if (!ast_VisitExpression(
                    expr->trystmt.trystmt[i], expr, visit_in, visit_out, ud
                    ))
                return 0;
            i++;
        }
        i = 0;
        while (i < expr->trystmt.exceptions_count) {
            if (!ast_VisitExpression(
                    expr->trystmt.exceptions[i], expr,
                    visit_in, visit_out, ud
                    ))
                return 0;
            i++;
        }
        i = 0;
        while (i < expr->trystmt.catchstmt_count) {
            if (!ast_VisitExpression(
                    expr->trystmt.catchstmt[i], expr, visit_in, visit_out, ud
                    ))
                return 0;
            i++;
        }
        i = 0;
        while (i < expr->trystmt.finallystmt_count) {
            if (!ast_VisitExpression(
                    expr->trystmt.finallystmt[i], expr,
                    visit_in, visit_out, ud
                    ))
                return 0;
            i++;
        }
        break;
    case H64EXPRTYPE_ASSIGN_STMT:
        if (!ast_VisitExpression(
                expr->assignstmt.lvalue, expr, visit_in, visit_out, ud
                ))
            return 0;
        if (!ast_VisitExpression(
                expr->assignstmt.rvalue, expr, visit_in, visit_out, ud
                ))
            return 0;
        break;
    case H64EXPRTYPE_BINARYOP:
        if (!ast_VisitExpression(
                expr->op.value1, expr, visit_in, visit_out, ud
                ))
            return 0;
        if (!ast_VisitExpression(
                expr->op.value2, expr, visit_in, visit_out, ud
                ))
            return 0;
        break;
    case H64EXPRTYPE_UNARYOP:
        if (!ast_VisitExpression(
                expr->op.value1, expr, visit_in, visit_out, ud
                ))
            return 0;
        break;
    case H64EXPRTYPE_CALL:
        if (!ast_VisitExpression(
                expr->inlinecall.value, expr, visit_in, visit_out, ud
                ))
            return 0;
        i = 0;
        while (i < expr->inlinecall.arguments.arg_count) {
            if (!ast_VisitExpression(
                    expr->inlinecall.arguments.arg_value[i], expr,
                    visit_in, visit_out, ud
                    ))
                return 0;
            i++;
        }
        break;
    case H64EXPRTYPE_LIST:
        i = 0;
        while (i < expr->constructorlist.entry_count) {
            if (!ast_VisitExpression(
                    expr->constructorlist.entry[i], expr,
                    visit_in, visit_out, ud
                    ))
                return 0;
            i++;
        }
        break;
    case H64EXPRTYPE_SET:
        i = 0;
        while (i < expr->constructorset.entry_count) {
            if (!ast_VisitExpression(
                    expr->constructorset.entry[i], expr,
                    visit_in, visit_out, ud
                    ))
                return 0;
            i++;
        }
        break;
    case H64EXPRTYPE_MAP:
        i = 0;
        while (i < expr->constructormap.entry_count) {
            if (!ast_VisitExpression(
                    expr->constructormap.key[i], expr,
                    visit_in, visit_out, ud
                    ))
                return 0;
            if (!ast_VisitExpression(
                    expr->constructormap.value[i], expr,
                    visit_in, visit_out, ud
                    ))
            i++;
        }
        break;
    case H64EXPRTYPE_VECTOR:
        i = 0;
        while (i < expr->constructorvector.entry_count) {
            if (!ast_VisitExpression(
                    expr->constructorvector.entry[i], expr,
                    visit_in, visit_out, ud
                    ))
                return 0;
            i++;
        }
        break;
    default:
        fprintf(stderr, "horsecc: warning: internal issue, "
            "unhandled expression in ast_VisitExpression(): "
            "type=%d, LIKELY BREAKAGE AHEAD.\n", expr->type);
    }

    if (visit_out) {
        if (!visit_out(expr, parent, ud))
            return 0;
    }

    return 1;
}


void ast_FreeExpression(h64expression *expr) {
    if (!expr)
        return;

    int i = 0;
    switch (expr->type) {
    case H64EXPRTYPE_INVALID:
        // nothing to do;
        break;
    case H64EXPRTYPE_VARDEF_STMT:
        if (expr->vardef.identifier)
            free(expr->vardef.identifier);
        if (expr->vardef.value)
            ast_FreeExpression(expr->vardef.value);
        break;
    case H64EXPRTYPE_FUNCDEF_STMT:
    case H64EXPRTYPE_INLINEFUNC:
        if (expr->funcdef.name)
            free(expr->funcdef.name);
        ast_ClearFunctionArgs(&expr->funcdef.arguments);
        int i = 0;
        while (i < expr->funcdef.stmt_count) {
            ast_FreeExpression(expr->funcdef.stmt[i]);
            i++;
        }
        if (expr->funcdef.stmt) free(expr->funcdef.stmt);
        ast_ClearFunctionArgs(&expr->funcdef.arguments);
        break;
    case H64EXPRTYPE_CALL_STMT:
        if (expr->callstmt.call) {
            if (expr->callstmt.call->inlinecall.value)
                ast_FreeExpression(
                    expr->callstmt.call->inlinecall.value
                );
            ast_ClearFunctionArgs(
                &expr->callstmt.call->inlinecall.arguments
            );
            free(expr->callstmt.call);
        }
        break;
    case H64EXPRTYPE_CLASSDEF_STMT:
        free(expr->classdef.name);
        ast_FreeExpression(expr->classdef.baseclass_ref);
        i = 0;
        while (i < expr->classdef.vardef_count) {
            ast_FreeExpression(expr->classdef.vardef[i]);
            i++;
        }
        free(expr->classdef.vardef);
        i = 0;
        while (i < expr->classdef.funcdef_count) {
            ast_FreeExpression(expr->classdef.funcdef[i]);
            i++;
        }
        free(expr->classdef.funcdef);
        break;
    case H64EXPRTYPE_IF_STMT: ;
        struct h64ifstmt *current_clause = &expr->ifstmt;
        while (current_clause) {
            struct h64ifstmt *next_clause = (
                current_clause->followup_clause
            );
            ast_FreeExpression(current_clause->conditional);
            i = 0;
            while (i < current_clause->stmt_count) {
                ast_FreeExpression(current_clause->stmt[i]);
                i++;
            }
            free(current_clause->stmt);
            current_clause = next_clause;
        }
        break;
    case H64EXPRTYPE_WHILE_STMT:
        ast_FreeExpression(expr->whilestmt.conditional);
        i = 0;
        while (i < expr->whilestmt.stmt_count) {
            ast_FreeExpression(expr->whilestmt.stmt[i]);
            i++;
        }
        free(expr->whilestmt.stmt);
        break;
    case H64EXPRTYPE_FOR_STMT:
        free(expr->forstmt.iterator_identifier);
        ast_FreeExpression(expr->forstmt.iterated_container);
        i = 0;
        while (i < expr->forstmt.stmt_count) {
            ast_FreeExpression(expr->forstmt.stmt[i]);
            i++;
        }
        free(expr->forstmt.stmt);
        break;
    case H64EXPRTYPE_IMPORT_STMT:
        i = 0;
        while (i < expr->importstmt.import_elements_count) {
            free(expr->importstmt.import_elements[i]);
            i++;
        }
        free(expr->importstmt.import_elements);
        free(expr->importstmt.import_as);
        break;
    case H64EXPRTYPE_RETURN_STMT:
        free(expr->returnstmt.returned_expression);
        break;
    case H64EXPRTYPE_TRY_STMT:
        i = 0;
        while (i < expr->trystmt.trystmt_count) {
            ast_FreeExpression(expr->trystmt.trystmt[i]);
            i++;
        }
        free(expr->trystmt.trystmt);
        i = 0;
        while (i < expr->trystmt.exceptions_count) {
            ast_FreeExpression(expr->trystmt.exceptions[i]);
            i++;
        }
        free(expr->trystmt.exceptions);
        free(expr->trystmt.exception_name);
        i = 0;
        while (i < expr->trystmt.catchstmt_count) {
            ast_FreeExpression(expr->trystmt.catchstmt[i]);
            i++;
        }
        free(expr->trystmt.catchstmt);
        i = 0;
        while (i < expr->trystmt.finallystmt_count) {
            ast_FreeExpression(expr->trystmt.finallystmt[i]);
            i++;
        }
        free(expr->trystmt.finallystmt);
        break;
    case H64EXPRTYPE_ASSIGN_STMT:
        ast_FreeExpression(expr->assignstmt.lvalue);
        ast_FreeExpression(expr->assignstmt.rvalue);
        break;
    case H64EXPRTYPE_LITERAL:
        if (expr->literal.type == H64TK_CONSTANT_STRING)
            free(expr->literal.str_value);
        break;
    case H64EXPRTYPE_IDENTIFIERREF:
        free(expr->identifierref.value);
        break;
    case H64EXPRTYPE_BINARYOP:
        ast_FreeExpression(expr->op.value1);
        ast_FreeExpression(expr->op.value2);
        break;
    case H64EXPRTYPE_UNARYOP:
        ast_FreeExpression(expr->op.value1);
        break;
    case H64EXPRTYPE_CALL:
        ast_FreeExpression(expr->inlinecall.value);
        ast_ClearFunctionArgs(&expr->inlinecall.arguments);
        break;
    case H64EXPRTYPE_LIST:
        i = 0;
        while (i < expr->constructorlist.entry_count) {
            ast_FreeExpression(expr->constructorlist.entry[i]);
            i++;
        }
        free(expr->constructorlist.entry);
        break;
    case H64EXPRTYPE_SET:
        i = 0;
        while (i < expr->constructorset.entry_count) {
            ast_FreeExpression(expr->constructorset.entry[i]);
            i++;
        }
        free(expr->constructorset.entry);
        break;
    case H64EXPRTYPE_MAP:
        i = 0;
        while (i < expr->constructormap.entry_count) {
            ast_FreeExpression(expr->constructormap.key[i]);
            i++;
        }
        free(expr->constructormap.key);
        i = 0;
        while (i < expr->constructormap.entry_count) {
            ast_FreeExpression(expr->constructormap.value[i]);
            i++;
        }
        free(expr->constructormap.value);
        break;
    case H64EXPRTYPE_VECTOR:
        i = 0;
        while (i < expr->constructorvector.entry_count) {
            ast_FreeExpression(expr->constructorvector.entry[i]);
            i++;
        }
        free(expr->constructorvector.entry);
        break;
    default:
        fprintf(stderr, "horsecc: warning: internal issue, "
            "unhandled expression in ast_FreeExpression(): "
            "type=%d, LIKELY MEMORY LEAK.\n", expr->type);
    }

    free(expr);
}

static char _h64exprname_invalid[] = "H64EXPRTYPE_INVALID";
static char _h64exprname_vardef_stmt[] = "H64EXPRTYPE_VARDEF_STMT";
static char _h64exprname_funcdef_stmt[] = "H64EXPRTYPE_FUNCDEF_STMT";
static char _h64exprname_call_stmt[] = "H64EXPRTYPE_CALL_STMT";
static char _h64exprname_classdef_stmt[] = "H64EXPRTYPE_CLASSDEF_STMT";
static char _h64exprname_if_stmt[] = "H64EXPRTYPE_IF_STMT";
static char _h64exprname_while_stmt[] = "H64EXPRTYPE_WHILE_STMT";
static char _h64exprname_for_stmt[] = "H64EXPRTYPE_FOR_STMT";
static char _h64exprname_import_stmt[] = "H64EXPRTYPE_IMPORT_STMT";
static char _h64exprname_try_stmt[] = "H64EXPRTYPE_TRY_STMT";
static char _h64exprname_assign_stmt[] = "H64EXPRTYPE_ASSIGN_STMT";
static char _h64exprname_literal[] = "H64EXPRTYPE_LITERAL";
static char _h64exprname_identifierref[] = "H64EXPRTYPE_IDENTIFIERREF";
static char _h64exprname_unaryop[] = "H64EXPRTYPE_UNARYOP";
static char _h64exprname_binaryop[] = "H64EXPRTYPE_BINARYOP";
static char _h64exprname_call[] = "H64EXPRTYPE_CALL";
static char _h64exprname_list[] = "H64EXPRTYPE_LIST";
static char _h64exprname_set[] = "H64EXPRTYPE_SET";
static char _h64exprname_map[] = "H64EXPRTYPE_MAP";
static char _h64exprname_vector[] = "H64EXPRTYPE_VECTOR";

const char *ast_ExpressionTypeToStr(h64expressiontype type) {
    if (type == H64EXPRTYPE_INVALID || type <= 0)
        return _h64exprname_invalid;
    switch (type) {
    case H64EXPRTYPE_VARDEF_STMT:
        return _h64exprname_vardef_stmt;
    case H64EXPRTYPE_FUNCDEF_STMT:
        return _h64exprname_funcdef_stmt;
    case H64EXPRTYPE_CALL_STMT:
        return _h64exprname_call_stmt;
    case H64EXPRTYPE_CLASSDEF_STMT:
        return _h64exprname_classdef_stmt;
    case H64EXPRTYPE_IF_STMT:
        return _h64exprname_if_stmt;
    case H64EXPRTYPE_WHILE_STMT:
        return _h64exprname_while_stmt;
    case H64EXPRTYPE_FOR_STMT:
        return _h64exprname_for_stmt;
    case H64EXPRTYPE_IMPORT_STMT:
        return _h64exprname_import_stmt;
    case H64EXPRTYPE_TRY_STMT:
        return _h64exprname_try_stmt;
    case H64EXPRTYPE_ASSIGN_STMT:
        return _h64exprname_assign_stmt;
    case H64EXPRTYPE_LITERAL:
        return _h64exprname_literal;
    case H64EXPRTYPE_IDENTIFIERREF:
        return _h64exprname_identifierref;
    case H64EXPRTYPE_UNARYOP:
        return _h64exprname_unaryop;
    case H64EXPRTYPE_BINARYOP:
        return _h64exprname_binaryop;
    case H64EXPRTYPE_CALL:
        return _h64exprname_call;
    case H64EXPRTYPE_LIST:
        return _h64exprname_list;
    case H64EXPRTYPE_SET:
        return _h64exprname_set;
    case H64EXPRTYPE_MAP:
        return _h64exprname_map;
    case H64EXPRTYPE_VECTOR:
        return _h64exprname_vector;
    default:
        return NULL;
    }
    return NULL;
}

char *ast_ExpressionToJSONStr(
        h64expression *e, const char *fileuri
        ) {
    jsonvalue *v = ast_ExpressionToJSON(e, fileuri);
    if (!v)
        return NULL;
    char *s = json_Dump(v);
    json_Free(v);
    return s;
}

jsonvalue *ast_FuncArgsToJSON(
        h64funcargs *fargs, const char *fileuri
        ) {
    jsonvalue *v = json_List();
    int fail = 0;
    int i = 0;
    while (i < fargs->arg_count) {
        jsonvalue *arg = json_Dict();
        if (!arg) {
            fail = 1;
            break;
        }
        if (fargs->arg_name[i] && strlen(fargs->arg_name[i]) > 0) {
            if (!json_SetDictStr(arg, "name", fargs->arg_name[i])) {
                fail = 1;
                json_Free(arg);
                break;
            }
        } else {
            if (!json_SetDictNull(arg, "name")) {
                fail = 1;
                json_Free(arg);
                break;
            }
        }
        if (fargs->arg_value[i]) {
            jsonvalue *innerv = ast_ExpressionToJSON(
                fargs->arg_value[i], fileuri
            );
            if (!innerv) {
                json_Free(arg);
                fail = 1;
                break;
            } else if (!json_SetDict(arg, "value", innerv)) {
                fail = 1;
                json_Free(innerv);
                json_Free(arg);
                break;
            }
        } else {
            if (!json_SetDictNull(arg, "value")) {
                fail = 1;
                json_Free(arg);
                break;
            }
        }
        if (!json_AddToList(v, arg)) {
            fail = 1;
            json_Free(arg);
            break;
        }
        i++;
    }
    if (fail) {
        json_Free(v);
        return NULL;
    }
    return v;
}

jsonvalue *ast_ExpressionToJSON(
        h64expression *e, const char *fileuri
        ) {
    if (!e)
        return NULL;
    int fail = 0;
    jsonvalue *v = json_Dict();
    const char *typestrconst = ast_ExpressionTypeToStr(e->type);
    char *typestr = typestrconst ? strdup(typestrconst) : NULL;
    if (typestr) {
        if (!json_SetDictStr(v, "type", typestr)) {
            fail = 1;
        }
        free(typestr);
    } else {
        fprintf(stderr, "horsecc: error: internal error, "
            "fail of handling expression type %d in "
            "ast_ExpressionTypeToStr\n",
            e->type);
        fail = 1;
    }
    if (e->line >= 0) {
        if (!json_SetDictInt(v, "line", e->line)) {
            fail = 1;
        } else if (e->column >= 0) {
            if (!json_SetDictInt(v, "column", e->column)) {
                fail = 1;
            }
        }
    }
    if (e->type == H64EXPRTYPE_VARDEF_STMT) {
        if (e->vardef.identifier &&
                !json_SetDictStr(v, "name", e->vardef.identifier))
            fail = 1;
        if (e->vardef.value) {
            jsonvalue *val = ast_ExpressionToJSON(e->vardef.value, fileuri);
            if (!val) {
                fail = 1;
            } else {
                if (!json_SetDict(v, "value", val)) {
                    json_Free(val);
                    fail = 1;
                }
            }
        }
    } else if (e->type == H64EXPRTYPE_CLASSDEF_STMT) {
        if (e->classdef.name &&
                !json_SetDictStr(v, "name", e->classdef.name))
            fail = 1;
        jsonvalue *vardefs = json_List();
        int i = 0;
        while (i < e->classdef.vardef_count) {
            jsonvalue *varjson = ast_ExpressionToJSON(
                e->classdef.vardef[i], fileuri
            );
            if (!json_AddToList(vardefs, varjson)) {
                json_Free(varjson);
                fail = 1;
                break;
            }
            i++;
        }
        jsonvalue *funcdefs = json_List();
        i = 0;
        while (i < e->classdef.funcdef_count) {
            jsonvalue *funcjson = ast_ExpressionToJSON(
                e->classdef.funcdef[i], fileuri
            );
            if (!json_AddToList(funcdefs, funcjson)) {
                json_Free(funcjson);
                fail = 1;
                break;
            }
            i++;
        }
        if (!json_SetDict(v, "variables", vardefs)) {
            fail = 1;
            json_Free(vardefs);
        }
        if (!json_SetDict(v, "functions", funcdefs)) {
            fail = 1;
            json_Free(funcdefs);
        }
    } else if (e->type == H64EXPRTYPE_FUNCDEF_STMT) {
        jsonvalue *attributes = json_List();
        if (e->funcdef.name &&
                !json_SetDictStr(v, "name", e->funcdef.name))
            fail = 1;
        jsonvalue *statements = json_List();
        int i = 0;
        while (i < e->funcdef.stmt_count) {
            jsonvalue *stmtjson = ast_ExpressionToJSON(
                e->funcdef.stmt[i], fileuri
            );
            if (!json_AddToList(statements, stmtjson)) {
                json_Free(stmtjson);
                fail = 1;
                break;
            }
            i++;
        }
        if (!json_SetDict(v, "statements", statements)) {
            fail = 1;
            json_Free(statements);
        }
        if (e->funcdef.is_threadable) {
            if (!json_AddToListStr(attributes, "threadable"))
                fail = 1;
        }
        if (!json_SetDict(v, "attributes", attributes)) {
            fail = 1;
            json_Free(attributes);
        }
        jsonvalue *value2 = ast_FuncArgsToJSON(
            &e->inlinecall.arguments, fileuri
        );
        if (!value2) {
            fail = 1;
        } else {
            if (!json_SetDict(v, "arguments", value2)) {
                json_Free(value2);
                fail = 1;
            }
        }
    } else if (e->type == H64EXPRTYPE_LITERAL) {
        if (e->literal.type == H64TK_CONSTANT_INT) {
            if (!json_SetDictInt(v, "value", e->literal.int_value))
                fail = 1;
        } else if (e->literal.type == H64TK_CONSTANT_FLOAT) {
            if (!json_SetDictFloat(v, "value", e->literal.float_value))
                fail = 1;
        } else if (e->literal.type == H64TK_CONSTANT_BOOL) {
            if (!json_SetDictBool(v, "value", (int)e->literal.int_value))
                fail = 1;
        } else if (e->literal.type == H64TK_CONSTANT_NONE) {
            if (!json_SetDictNull(v, "value"))
                fail = 1;
        } else if (e->literal.type == H64TK_CONSTANT_STRING) {
            if (!json_SetDictStr(v, "value", e->literal.str_value))
                fail = 1;
        }
    } else if (e->type == H64EXPRTYPE_BINARYOP) {
        jsonvalue *value1 = ast_ExpressionToJSON(
            e->op.value1, fileuri
        );
        if (!value1) {
            fail = 1;
        } else {
            if (!json_SetDict(v, "operand1", value1)) {
                json_Free(value1);
                fail = 1;
            }
        }
        jsonvalue *value2 = ast_ExpressionToJSON(
            e->op.value2, fileuri
        );
        if (!value2) {
            fail = 1;
        } else {
            if (!json_SetDict(v, "operand2", value2)) {
                json_Free(value2);
                fail = 1;
            }
        }
        if (!json_SetDictStr(v, "operator",
                operator_OpTypeToStr(e->op.optype)))
            fail = 1;
    } else if (e->type == H64EXPRTYPE_UNARYOP) {
        jsonvalue *value1 = ast_ExpressionToJSON(
            e->op.value1, fileuri
        );
        if (!value1) {
            fail = 1;
        } else {
            if (!json_SetDict(v, "operand", value1)) {
                json_Free(value1);
                fail = 1;
            }
        }
    } else if (e->type == H64EXPRTYPE_CALL ||
            e->type == H64EXPRTYPE_CALL_STMT) {
        h64expression *innere = (
            e->type == H64EXPRTYPE_CALL ? e :
            e->callstmt.call
        );
        if (innere->inlinecall.value != NULL) {
            jsonvalue *value1 = ast_ExpressionToJSON(
                innere->inlinecall.value, fileuri
            );
            if (!value1) {
                fail = 1;
            } else {
                if (!json_SetDict(v, "callee", value1)) {
                    json_Free(value1);
                    fail = 1;
                }
            }
        }
        jsonvalue *value2 = ast_FuncArgsToJSON(
            &innere->inlinecall.arguments, fileuri
        );
        if (!value2) {
            fail = 1;
        } else {
            if (!json_SetDict(v, "arguments", value2)) {
                json_Free(value2);
                fail = 1;
            }
        }
    }
    if (fileuri) {
        if (!json_SetDictStr(v, "file-uri", fileuri))
            fail = 1;
    }
    if (fail) {
        json_Free(v);
        return NULL;
    }
    return v;
}
