#ifndef __KSU_H_APK_V2_SIGN
#define __KSU_H_APK_V2_SIGN

#include <linux/types.h>

#ifndef KSU_MAX_PACKAGE_NAME
#define KSU_MAX_PACKAGE_NAME 256
#endif
#ifndef KSU_MANAGER_PACKAGE
#define KSU_MANAGER_PACKAGE ""
#endif
#ifndef EXPECTED_SIZE
#define EXPECTED_SIZE 0
#endif
#ifndef EXPECTED_HASH
#define EXPECTED_HASH NULL
#endif
#ifndef EXPECTED_SIZE2
#define EXPECTED_SIZE2 0
#endif
#ifndef EXPECTED_HASH2
#define EXPECTED_HASH2 NULL
#endif

bool is_manager_apk(char *path);
int get_pkg_from_apk_path(char *pkg, const char *path);

#endif
