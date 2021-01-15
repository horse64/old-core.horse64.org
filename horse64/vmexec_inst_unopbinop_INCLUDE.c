// Copyright (c) 2020, ellie/@ell1e & Horse64 Team (see AUTHORS.md),
// also see LICENSE.md file.
// SPDX-License-Identifier: BSD-2-Clause

// NOTE:
// THIS FILE IS INLINE-INCLUDED BY VMEXEC.C INTO THE
// _vmthread_RunFunction_NoPopFuncFrames FUNCTION.
// IT IS **NOT** A SEPARATE OBJECT FILE.
// It was split up here to make this code easier to work with,
// given how giant the source function is in vmexec.c.

    #include "gcvalue.h"
#include "valuecontentstruct.h"
#include "vmstrings.h"
#include "widechar.h"
inst_binop: {
        h64instruction_binop *inst = (h64instruction_binop *)p;
        #ifndef NDEBUG
        if (vmthread->vmexec_owner->moptions.vmexec_debug &&
                !vmthread_PrintExec(vmthread, func_id, (void*)inst))
            goto triggeroom;
        #endif

        // Silence false positive warnings:
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

        int copyatend = 0;
        valuecontent _tmpresultbuf = {0};
        valuecontent *tmpresult = STACK_ENTRY(stack, inst->slotto);
        if (likely(inst->slotto == inst->arg1slotfrom ||
                inst->slotto == inst->arg2slotfrom)) {
            copyatend = 1;
            tmpresult = &_tmpresultbuf;
        } else {
            DELREF_NONHEAP(tmpresult);
            valuecontent_Free(tmpresult);
            memset(tmpresult, 0, sizeof(*tmpresult));
        }

        valuecontent *v1 = STACK_ENTRY(stack, inst->arg1slotfrom);
        valuecontent *v2 = STACK_ENTRY(stack, inst->arg2slotfrom);
        #ifndef NDEBUG
        if (!op_jumptable[inst->optype]) {
            h64fprintf(
                stderr, "binop %d missing in jump table\n",
                inst->optype
            );
            return 0;
        }
        #endif
        int invalidtypes = 1;
        int divisionbyzero = 0;
        goto *op_jumptable[inst->optype];
        binop_divide: {
            if (unlikely((v1->type != H64VALTYPE_INT64 &&
                    v1->type != H64VALTYPE_FLOAT64) ||
                    (v2->type != H64VALTYPE_INT64 &&
                    v2->type != H64VALTYPE_FLOAT64))) {
                // invalid
            } else {
                invalidtypes = 0;
                if (v1->type == H64VALTYPE_FLOAT64 ||
                        v2->type == H64VALTYPE_FLOAT64) {
                    double v1no = 1;
                    if (v1->type == H64VALTYPE_FLOAT64) {
                        v1no = v1->float_value;
                    } else {
                        v1no = v1->int_value;
                    }
                    double v2no = 1;
                    if (v2->type == H64VALTYPE_FLOAT64) {
                        v2no = v2->float_value;
                    } else {
                        v2no = v2->int_value;
                    }
                    tmpresult->type = H64VALTYPE_FLOAT64;
                    tmpresult->float_value = (v1no / v2no);
                    if (isnan(tmpresult->float_value) || v2no == 0) {
                        divisionbyzero = 1;
                    }
                } else {
                    tmpresult->type = H64VALTYPE_INT64;
                    if (v2->int_value == 0) {
                        divisionbyzero = 1;
                    } else {
                        tmpresult->int_value = (
                            v1->int_value / v2->int_value
                        );
                    }
                }
            }
            goto binop_done;
        }
        binop_add: {
            if (unlikely((v1->type != H64VALTYPE_INT64 &&
                    v1->type != H64VALTYPE_FLOAT64) ||
                    (v2->type != H64VALTYPE_INT64 &&
                    v2->type != H64VALTYPE_FLOAT64))) {
                if (likely((
                        ((v1->type == H64VALTYPE_GCVAL &&
                         ((h64gcvalue *)v1->ptr_value)->type ==
                            H64GCVALUETYPE_STRING) ||
                         v1->type == H64VALTYPE_SHORTSTR)) &&
                        ((v2->type == H64VALTYPE_GCVAL &&
                         ((h64gcvalue *)v2->ptr_value)->type ==
                            H64GCVALUETYPE_STRING) ||
                         v2->type == H64VALTYPE_SHORTSTR))) { // string concat
                    invalidtypes = 0;
                    int64_t len1 = -1;
                    char *ptr1 = NULL;
                    if (v1->type == H64VALTYPE_SHORTSTR) {
                        len1 = v1->shortstr_len;
                        ptr1 = (char*)v1->shortstr_value;
                    } else {
                        len1 = (
                            (int64_t)((h64gcvalue *)v1->ptr_value)->
                            str_val.len
                        );
                        ptr1 = (char*)(
                            (int64_t)((h64gcvalue *)v1->ptr_value)->
                            str_val.s
                        );
                    }
                    int64_t len2 = -1;
                    char *ptr2 = NULL;
                    if (v2->type == H64VALTYPE_SHORTSTR) {
                        len2 = v2->shortstr_len;
                        ptr2 = (char*)v2->shortstr_value;
                    } else {
                        len2 = (
                            (int64_t)((h64gcvalue *)v2->ptr_value)->
                            str_val.len
                        );
                        ptr2 = (char*)(
                            (int64_t)((h64gcvalue *)v2->ptr_value)->
                            str_val.s
                        );
                    }
                    if (len1 + len2 <= VALUECONTENT_SHORTSTRLEN) {
                        tmpresult->type = H64VALTYPE_SHORTSTR;
                        tmpresult->shortstr_len = len1 + len2;
                        if (len1 > 0) {
                            memcpy(
                                tmpresult->shortstr_value,
                                ptr1, len1 * sizeof(h64wchar)
                            );
                        }
                        if (len2 > 0) {
                            memcpy(
                                tmpresult->shortstr_value +
                                len1 * sizeof(h64wchar),
                                ptr2, len2 * sizeof(h64wchar)
                            );
                        }
                    } else {
                        tmpresult->type = H64VALTYPE_GCVAL;
                        h64gcvalue *gcval = poolalloc_malloc(
                            heap, 0
                        );
                        if (!gcval) {
                            tmpresult->ptr_value = NULL;
                            goto triggeroom;
                        }
                        tmpresult->ptr_value = gcval;
                        gcval->hash = 0;
                        gcval->type = H64GCVALUETYPE_STRING;
                        gcval->heapreferencecount = 0;
                        gcval->externalreferencecount = 1;
                        memset(&gcval->str_val, 0,
                               sizeof(gcval->str_val));
                        if (!vmstrings_AllocBuffer(
                                vmthread, &gcval->str_val,
                                len1 + len2)) {
                            poolalloc_free(heap, gcval);
                            tmpresult->ptr_value = NULL;
                            goto triggeroom;
                        }
                        if (len1 > 0) {
                            memcpy(
                                gcval->str_val.s,
                                ptr1, len1 * sizeof(h64wchar)
                            );
                        }
                        if (len2 > 0) {
                            memcpy(
                                ((char*)gcval->str_val.s) +
                                len1 * sizeof(h64wchar),
                                ptr2, len2 * sizeof(h64wchar)
                            );
                        }
                    }
                } else {
                    // invalid
                }
            } else {  // number addition
                invalidtypes = 0;
                if (v1->type == H64VALTYPE_FLOAT64 ||
                        v2->type == H64VALTYPE_FLOAT64) {
                    double v1no = 1;
                    if (v1->type == H64VALTYPE_FLOAT64) {
                        v1no = v1->float_value;
                    } else {
                        v1no = v1->int_value;
                    }
                    double v2no = 1;
                    if (v2->type == H64VALTYPE_FLOAT64) {
                        v2no = v2->float_value;
                    } else {
                        v2no = v2->int_value;
                    }
                    tmpresult->type = H64VALTYPE_FLOAT64;
                    tmpresult->float_value = (v1no + v2no);
                } else {
                    tmpresult->type = H64VALTYPE_INT64;
                    tmpresult->int_value = (
                        v1->int_value + v2->int_value
                    );
                }
            }
            goto binop_done;
        }
        binop_substract: {
            if (unlikely((v1->type != H64VALTYPE_INT64 &&
                    v1->type != H64VALTYPE_FLOAT64) ||
                    (v2->type != H64VALTYPE_INT64 &&
                    v2->type != H64VALTYPE_FLOAT64))) {
                // invalid
            } else {
                invalidtypes = 0;
                if (v1->type == H64VALTYPE_FLOAT64 ||
                        v2->type == H64VALTYPE_FLOAT64) {
                    double v1no = 1;
                    if (v1->type == H64VALTYPE_FLOAT64) {
                        v1no = v1->float_value;
                    } else {
                        v1no = v1->int_value;
                    }
                    double v2no = 1;
                    if (v2->type == H64VALTYPE_FLOAT64) {
                        v2no = v2->float_value;
                    } else {
                        v2no = v2->int_value;
                    }
                    tmpresult->type = H64VALTYPE_FLOAT64;
                    tmpresult->float_value = (v1no - v2no);
                } else {
                    tmpresult->type = H64VALTYPE_INT64;
                    tmpresult->int_value = (
                        v1->int_value - v2->int_value
                    );
                }
            }
            goto binop_done;
        }
        binop_multiply: {
            if (unlikely((v1->type != H64VALTYPE_INT64 &&
                    v1->type != H64VALTYPE_FLOAT64) ||
                    (v2->type != H64VALTYPE_INT64 &&
                    v2->type != H64VALTYPE_FLOAT64))) {
                // invalid
            } else {
                invalidtypes = 0;
                if (v1->type == H64VALTYPE_FLOAT64 ||
                        v2->type == H64VALTYPE_FLOAT64) {
                    double v1no = 1;
                    if (v1->type == H64VALTYPE_FLOAT64) {
                        v1no = v1->float_value;
                    } else {
                        v1no = v1->int_value;
                    }
                    double v2no = 1;
                    if (v2->type == H64VALTYPE_FLOAT64) {
                        v2no = v2->float_value;
                    } else {
                        v2no = v2->int_value;
                    }
                    tmpresult->type = H64VALTYPE_FLOAT64;
                    tmpresult->float_value = (v1no * v2no);
                } else {
                    tmpresult->type = H64VALTYPE_INT64;
                    tmpresult->int_value = (
                        v1->int_value * v2->int_value
                    );
                }
            }
            goto binop_done;
        }
        binop_modulo: {
            if (unlikely((v1->type != H64VALTYPE_INT64 &&
                    v1->type != H64VALTYPE_FLOAT64) ||
                    (v2->type != H64VALTYPE_INT64 &&
                    v2->type != H64VALTYPE_FLOAT64))) {
                // invalid
            } else {
                invalidtypes = 0;
                if (v1->type == H64VALTYPE_FLOAT64 ||
                        v2->type == H64VALTYPE_FLOAT64) {
                    double v1no = 1;
                    if (v1->type == H64VALTYPE_FLOAT64) {
                        v1no = v1->float_value;
                    } else {
                        v1no = v1->int_value;
                    }
                    double v2no = 1;
                    if (v2->type == H64VALTYPE_FLOAT64) {
                        v2no = v2->float_value;
                    } else {
                        v2no = v2->int_value;
                    }
                    tmpresult->type = H64VALTYPE_FLOAT64;
                    if (unlikely(v1no < 0 || v2no < 0)) {
                        if (v1no >= 0 && v2no < 0) {
                            tmpresult->float_value = -(
                                (-v2no) - fmod(v1no, -v2no)
                            );
                        } else if (v2no >= 0) {
                            tmpresult->float_value = (
                                v2no - fmod(-v1no, v2no)
                            );
                        } else {
                            tmpresult->float_value = (
                                -fmod(-v1no, -v2no)
                            );
                        }
                    } else {
                        tmpresult->float_value = fmod(v1no, v2no);
                    }
                    if (isnan(tmpresult->float_value) || v2no == 0) {
                        divisionbyzero = 1;
                    }
                } else {
                    tmpresult->type = H64VALTYPE_INT64;
                    if (v2->int_value == 0) {
                        divisionbyzero = 1;
                    } else {
                        tmpresult->int_value = (
                            v1->int_value % v2->int_value
                        );
                    }
                }
            }
            goto binop_done;
        }
        binop_cmp_equal: {
            invalidtypes = 0;
            tmpresult->type = H64VALTYPE_BOOL;
            int result = 0;
            int success = (
                vmexec_ValueEqualityCheck(
                    vmthread, v1, v2, &result
                )
            );
            if (!success) {
                goto triggeroom;
            }
            tmpresult->int_value = (result != 0);
            goto binop_done;
        }
        binop_cmp_notequal: {
            h64fprintf(stderr, "oopsie daisy\n");
            return 0;
        }
        binop_cmp_largerorequal: {
            if (likely((v1->type != H64VALTYPE_INT64 &&
                    v1->type != H64VALTYPE_FLOAT64) ||
                    (v2->type != H64VALTYPE_INT64 &&
                    v2->type != H64VALTYPE_FLOAT64))) {
                // invalid
            } else {
                invalidtypes = 0;
                if (v1->type == H64VALTYPE_FLOAT64 ||
                        v2->type == H64VALTYPE_FLOAT64) {
                    double v1no = 1;
                    if (v1->type == H64VALTYPE_FLOAT64) {
                        v1no = v1->float_value;
                    } else {
                        v1no = v1->int_value;
                    }
                    double v2no = 1;
                    if (v2->type == H64VALTYPE_FLOAT64) {
                        v2no = v2->float_value;
                    } else {
                        v2no = v2->int_value;
                    }
                    tmpresult->type = H64VALTYPE_BOOL;
                    tmpresult->int_value = (v1no >= v2no);
                } else {
                    tmpresult->type = H64VALTYPE_BOOL;
                    tmpresult->int_value = (
                        v1->int_value >= v2->int_value
                    );
                }
            }
            goto binop_done;
        }
        binop_cmp_smallerorequal: {
            if (likely((v1->type != H64VALTYPE_INT64 &&
                    v1->type != H64VALTYPE_FLOAT64) ||
                    (v2->type != H64VALTYPE_INT64 &&
                    v2->type != H64VALTYPE_FLOAT64))) {
                // invalid
            } else {
                invalidtypes = 0;
                if (v1->type == H64VALTYPE_FLOAT64 ||
                        v2->type == H64VALTYPE_FLOAT64) {
                    double v1no = 1;
                    if (v1->type == H64VALTYPE_FLOAT64) {
                        v1no = v1->float_value;
                    } else {
                        v1no = v1->int_value;
                    }
                    double v2no = 1;
                    if (v2->type == H64VALTYPE_FLOAT64) {
                        v2no = v2->float_value;
                    } else {
                        v2no = v2->int_value;
                    }
                    tmpresult->type = H64VALTYPE_BOOL;
                    tmpresult->int_value = (v1no <= v2no);
                } else {
                    tmpresult->type = H64VALTYPE_BOOL;
                    tmpresult->int_value = (
                        v1->int_value <= v2->int_value
                    );
                }
            }
            goto binop_done;
        }
        binop_cmp_larger: {
            if (likely((v1->type != H64VALTYPE_INT64 &&
                    v1->type != H64VALTYPE_FLOAT64) ||
                    (v2->type != H64VALTYPE_INT64 &&
                    v2->type != H64VALTYPE_FLOAT64))) {
                // invalid
            } else {
                invalidtypes = 0;
                if (v1->type == H64VALTYPE_FLOAT64 ||
                        v2->type == H64VALTYPE_FLOAT64) {
                    double v1no = 1;
                    if (v1->type == H64VALTYPE_FLOAT64) {
                        v1no = v1->float_value;
                    } else {
                        v1no = v1->int_value;
                    }
                    double v2no = 1;
                    if (v2->type == H64VALTYPE_FLOAT64) {
                        v2no = v2->float_value;
                    } else {
                        v2no = v2->int_value;
                    }
                    tmpresult->type = H64VALTYPE_BOOL;
                    tmpresult->int_value = (v1no > v2no);
                } else {
                    tmpresult->type = H64VALTYPE_BOOL;
                    tmpresult->int_value = (
                        v1->int_value > v2->int_value
                    );
                }
            }
            goto binop_done;
        }
        binop_cmp_smaller: {
            if (unlikely((v1->type != H64VALTYPE_INT64 &&
                    v1->type != H64VALTYPE_FLOAT64) ||
                    (v2->type != H64VALTYPE_INT64 &&
                    v2->type != H64VALTYPE_FLOAT64))) {
                // invalid
            } else {
                invalidtypes = 0;
                if (v1->type == H64VALTYPE_FLOAT64 ||
                        v2->type == H64VALTYPE_FLOAT64) {
                    double v1no = 1;
                    if (v1->type == H64VALTYPE_FLOAT64) {
                        v1no = v1->float_value;
                    } else {
                        v1no = v1->int_value;
                    }
                    double v2no = 1;
                    if (v2->type == H64VALTYPE_FLOAT64) {
                        v2no = v2->float_value;
                    } else {
                        v2no = v2->int_value;
                    }
                    tmpresult->type = H64VALTYPE_BOOL;
                    tmpresult->int_value = (v1no < v2no);
                } else {
                    tmpresult->type = H64VALTYPE_BOOL;
                    tmpresult->int_value = (
                        v1->int_value < v2->int_value
                    );
                }
            }
            goto binop_done;
        }
        binop_boolcond_and: {
            int bool1, bool2;
            if (!_vmexec_CondExprValue(v1, &bool1)) {
                RAISE_ERROR(
                    H64STDERROR_TYPEERROR,
                    "this value type cannot be "
                    "evaluated as conditional"
                );
                goto *jumptable[((h64instructionany *)p)->type];
            }
            if (!bool1) {
                invalidtypes = 0;
                tmpresult->type = H64VALTYPE_BOOL;
                tmpresult->int_value = 0;
                goto binop_done;
            } else {
                if (!_vmexec_CondExprValue(v1, &bool2)) {
                    RAISE_ERROR(
                        H64STDERROR_TYPEERROR,
                        "this value type cannot be "
                        "evaluated as conditional"
                    );
                    goto *jumptable[((h64instructionany *)p)->type];
                }
                invalidtypes = 0;
                tmpresult->type = H64VALTYPE_BOOL;
                tmpresult->int_value = bool2;
                goto binop_done;
            }
        }
        binop_boolcond_or: {
            int bool1, bool2;
            if (!_vmexec_CondExprValue(v1, &bool1)) {
                RAISE_ERROR(
                    H64STDERROR_TYPEERROR,
                    "this value type cannot be "
                    "evaluated as conditional"
                );
                goto *jumptable[((h64instructionany *)p)->type];
            }
            if (bool1) {
                invalidtypes = 0;
                tmpresult->type = H64VALTYPE_BOOL;
                tmpresult->int_value = 1;
                goto binop_done;
            } else {
                if (!_vmexec_CondExprValue(v1, &bool2)) {
                    RAISE_ERROR(
                        H64STDERROR_TYPEERROR,
                        "this value type cannot be "
                        "evaluated as conditional"
                    );
                    goto *jumptable[((h64instructionany *)p)->type];
                }
                invalidtypes = 0;
                tmpresult->type = H64VALTYPE_BOOL;
                tmpresult->int_value = bool2;
                goto binop_done;
            }
        }
        binop_indexbyexpr: {
            int64_t index_by = -1;
            if (v1->type != H64VALTYPE_GCVAL || (
                    ((h64gcvalue *)v1->ptr_value)->type !=
                    H64GCVALUETYPE_MAP
                    )) {
                // Index must be int, so convert here:
                if (unlikely(v2->type != H64VALTYPE_INT64 &&
                        v2->type != H64VALTYPE_FLOAT64)) {
                    RAISE_ERROR(
                        H64STDERROR_TYPEERROR,
                        "this value must be indexed with a number"
                    );
                    goto *jumptable[((h64instructionany *)p)->type];
                }
                if (likely(v2->type == H64VALTYPE_INT64)) {
                    index_by = v2->int_value;
                } else {
                    assert(v2->type == H64VALTYPE_FLOAT64);
                    index_by = roundl(v2->float_value);
                }
            }
            invalidtypes = 0;
            if (v1->type == H64VALTYPE_GCVAL && (
                    ((h64gcvalue *)v1->ptr_value)->type ==
                    H64GCVALUETYPE_LIST
                    )) {
                valuecontent *v = vmlist_Get(
                    ((h64gcvalue *)v1->ptr_value)->list_values, index_by
                );
                if (!v) {
                    RAISE_ERROR(
                        H64STDERROR_INDEXERROR,
                        "index %" PRId64 " is out of range",
                        (int64_t)index_by
                    );
                    goto *jumptable[((h64instructionany *)p)->type];
                }
                memcpy(tmpresult, v, sizeof(*v));
                ADDREF_NONHEAP(tmpresult);
            } else if (v1->type == H64VALTYPE_GCVAL && (
                    ((h64gcvalue *)v1->ptr_value)->type ==
                    H64GCVALUETYPE_MAP
                    )) {
                valuecontent v = {0};
                if (!vmmap_Get(
                        ((h64gcvalue *)v1->ptr_value)->map_values,
                        v2, &v
                        )) {
                    RAISE_ERROR(
                        H64STDERROR_INDEXERROR,
                        "key not found in map"
                    );
                    goto *jumptable[((h64instructionany *)p)->type];
                }
                memcpy(tmpresult, &v, sizeof(v));
                ADDREF_NONHEAP(tmpresult);
            } else if ((v1->type == H64VALTYPE_GCVAL &&
                    ((h64gcvalue *)v1->ptr_value)->type ==
                    H64GCVALUETYPE_STRING
                    ) || v1->type == H64VALTYPE_CONSTPREALLOCSTR ||
                    v1->type == H64VALTYPE_SHORTSTR) {
                char *s = NULL;
                int64_t slen = -1;
                int64_t sletters = -1;
                if (v1->type == H64VALTYPE_GCVAL) {
                    assert(
                        ((h64gcvalue *)v1->ptr_value)->type ==
                        H64GCVALUETYPE_STRING
                    );
                    s = (char *)((h64gcvalue *)v1->ptr_value)->str_val.s;
                    slen = ((h64gcvalue *)v1->ptr_value)->str_val.len;
                    vmstrings_RequireLetterLen(
                        &(((h64gcvalue *)v1->ptr_value)->str_val)
                    );
                    sletters = ((h64gcvalue *)v1->ptr_value)->str_val.
                        letterlen;
                } else {
                    if (v1->type == H64VALTYPE_CONSTPREALLOCSTR) {
                        s = (char *)v1->constpreallocstr_value;
                        slen = v1->constpreallocstr_len;
                    } else {
                        s = (char *)v1->shortstr_value;
                        slen = v1->shortstr_len;
                    }
                    sletters = utf32_letters_count((h64wchar *)s, slen);
                }
                if (index_by < 1 || index_by > sletters) {
                    RAISE_ERROR(
                        H64STDERROR_INDEXERROR,
                        "index %" PRId64 " is out of range",
                        (int64_t)index_by
                    );
                }
                while (index_by > 1) {
                    int64_t len = utf32_letter_len((h64wchar *)s, slen);
                    assert(len > 0);
                    s += len * sizeof(h64wchar);
                    // ^ Multiply, since char * (not h64wchar *)
                    slen -= len;
                    sletters--;
                    index_by--;
                }
                if (!valuecontent_SetStringU32(
                        vmthread, tmpresult,
                        (h64wchar *)s,
                        utf32_letter_len((h64wchar *)s, slen)
                        )) {
                    RAISE_ERROR(
                        H64STDERROR_OUTOFMEMORYERROR,
                        "alloc failure creating result string"
                    );
                }
                ADDREF_NONHEAP(tmpresult);
            } else {
                RAISE_ERROR(
                    H64STDERROR_TYPEERROR,
                    "given value cannot be indexed"
                );
                goto *jumptable[((h64instructionany *)p)->type];
            }
            goto binop_done;
        }
        binop_done:
        if (invalidtypes) {
            RAISE_ERROR(
                H64STDERROR_TYPEERROR,
                "cannot apply %s operator to given types",
                operator_OpPrintedAsStr(inst->optype)
            );
            goto *jumptable[((h64instructionany *)p)->type];
        } else if (divisionbyzero) {
            RAISE_ERROR(
                H64STDERROR_MATHERROR,
                "division by zero"
            );
            goto *jumptable[((h64instructionany *)p)->type];
        }
        if (copyatend) {
            valuecontent *target = STACK_ENTRY(stack, inst->slotto);
            DELREF_NONHEAP(target);
            valuecontent_Free(target);
            memcpy(target, tmpresult, sizeof(*tmpresult));
        }
        #pragma GCC diagnostic pop
        p += sizeof(h64instruction_binop);
        goto *jumptable[((h64instructionany *)p)->type];
    }
    inst_unop: {
        h64instruction_unop *inst = (h64instruction_unop *)p;
        #ifndef NDEBUG
        if (vmthread->vmexec_owner->moptions.vmexec_debug &&
                !vmthread_PrintExec(vmthread, func_id, (void*)inst))
            goto triggeroom;
        #endif

        int copyatend = 0;
        valuecontent _tmpresultbuf = {0};
        valuecontent *tmpresult = STACK_ENTRY(stack, inst->slotto);
        if (likely(inst->slotto == inst->argslotfrom)) {
            copyatend = 1;
            tmpresult = &_tmpresultbuf;
        } else {
            DELREF_NONHEAP(tmpresult);
            valuecontent_Free(tmpresult);
            memset(tmpresult, 0, sizeof(*tmpresult));
        }

        valuecontent *v1 = STACK_ENTRY(stack, inst->argslotfrom);

        if (inst->optype == H64OP_BOOLCOND_NOT) {
            int boolv;
            if (!_vmexec_CondExprValue(v1, &boolv)) {
                RAISE_ERROR(
                    H64STDERROR_TYPEERROR,
                    "this value type cannot be "
                    "evaluated as conditional"
                );
                goto *jumptable[((h64instructionany *)p)->type];
            }
            tmpresult->type = H64VALTYPE_BOOL;
            tmpresult->int_value = !boolv;
            goto unop_done;
        } else {
            h64fprintf(stderr, "unop not implemented\n");
            return 0;
        }
        unop_done:
        if (copyatend) {
            valuecontent *target = STACK_ENTRY(stack, inst->slotto);
            DELREF_NONHEAP(target);
            valuecontent_Free(target);
            memcpy(target, tmpresult, sizeof(*tmpresult));
        }
        p += sizeof(h64instruction_unop);
        goto *jumptable[((h64instructionany *)p)->type];
    }
