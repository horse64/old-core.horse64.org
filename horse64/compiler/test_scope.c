#include <assert.h>
#include <check.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>

#include "compiler/ast.h"
#include "compiler/astparser.h"
#include "compiler/compileproject.h"
#include "filesys.h"
#include "compiler/scoperesolver.h"
#include "vfs.h"

#include "../testmain.h"

START_TEST (test_scope_import_complex)
{
    vfs_Init(NULL);

    h64compilewarnconfig wconfig;
    memset(&wconfig, 0, sizeof(wconfig));
    warningconfig_Init(&wconfig);

    char *cwd = filesys_GetCurrentDirectory();
    assert(cwd != NULL);
    char *testfolder_path = filesys_Join(cwd, ".testdata-prj");
    assert(testfolder_path != NULL);

    if (filesys_FileExists(testfolder_path)) {
        ck_assert(filesys_IsDirectory(testfolder_path));
        int result = filesys_RemoveFolder(testfolder_path, 1);
        assert(result != 0);
    }
    assert(filesys_FileExists(testfolder_path) == 0);
    int createresult = filesys_CreateDirectory(testfolder_path);
    ck_assert(createresult);
    createresult = filesys_CreateDirectory(".testdata-prj/horse_modules");
    ck_assert(createresult);
    createresult = filesys_CreateDirectory(
        ".testdata-prj/horse_modules/mylib"
    );
    ck_assert(createresult);
    createresult = filesys_CreateDirectory(
        ".testdata-prj/horse_modules/mylib/mymodule"
    );
    ck_assert(createresult);

    h64compileproject *project = compileproject_New(
        testfolder_path
    );
    assert(project != NULL);

    {
        FILE *f = fopen(".testdata-prj/mainfile.h64", "wb");
        ck_assert(f != NULL);
        char s[] = (
            "import mymodule.test1 @lib mylib\n"
            "import mymodule.test2 @lib mylib\n"
            "class TestClass {"
            "    var v = 1.5 + 0xA + 0b10"
            "}"
            "func main {"
            "    var obj = TestClass()"
            "}"
        );
        ck_assert(fwrite(s, 1, strlen(s), f));
        fclose(f);
    }
    {
        FILE *f = fopen(
            ".testdata-prj/horse_modules/mylib/mymodule/test1.h64",
            "wb"
        );
        ck_assert(f != NULL);
        char s[] = (
            "func test {"
            "    print(\"test\")"
            "}"
        );
        ck_assert(fwrite(s, 1, strlen(s), f));
        fclose(f);
    }
    {
        FILE *f = fopen(
            ".testdata-prj/horse_modules/mylib/mymodule/test2.h64",
            "wb"
        );
        ck_assert(f != NULL);
        char s[] = (
            "func test {"
            "    print(\"test\")"
            "}"
        );
        ck_assert(fwrite(s, 1, strlen(s), f));
        fclose(f);
    }

    char *error = NULL;
    h64ast *ast = NULL;
    ck_assert(compileproject_GetAST(
        project, ".testdata/mainfile.h64", &ast, &error
    ) != 0);
    ck_assert(error == NULL);
    ck_assert(scoperesolver_ResolveAST(project, ast) != 0);

    compileproject_Free(project);  // This indirectly frees 'ast'!
}
END_TEST

TESTS_MAIN (test_scope_import_complex)
