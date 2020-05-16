//#include "finesse-internal.h"
//#include <stdarg.h>
//#include <uuid/uuid.h>
//#include <sys/stat.h>

//static int fin_mkdir(const char *path, mode_t mode) {
//    typedef int (*orig_mkdir_t)(const char *path, mode_t mode);
//    static orig_mkdir_t orig_mkdir = NULL;
//
//    if (NULL == orig_mkdir) {
//        orig_mkdir = (orig_mkdir_t)dlsym(RTLD_NEXT, "mkdir");
//
//        assert(NULL != orig_mkdir);
//        if (NULL == orig_mkdir) {
//            return EACCES;
//        }
//    }
//
//    return orig_mkdir(path, mode);
//}
//
//static int fin_mkdirat(int fd, const char *path, mode_t mode) {
//    typedef int (*orig_mkdirat_t)(int fd, const char *path, mode_t mode);
//    static orig_mkdirat_t orig_mkdirat = NULL;
//
//    if (NULL == orig_mkdirat) {
//        orig_mkdirat = (orig_mkdirat_t)dlsym(RTLD_NEXT, "mkdirat");
//
//        assert(NULL != orig_mkdirat);
//        if (NULL == orig_mkdirat) {
//            return EACCES;
//        }
//    }
//
//    return orig_mkdirat(fd, path, mode);
//}
//
//int finesse_mkdir(const char *path, mode_t mode) {
//    struct stat sb;
//
//    finesse_init();
//    int status;
//
//    if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
//        // Directory already exists
//        return -1;
//    }
//
//    status = fin_mkdir(path, mode);
//
//    return status;
//}
//
//int finesse_mkdirat(int fd, const char *path, mode_t mode) {
//    int status;
//    
//    fd = fin_mkdirat(fd, path, mode);
//
//    status = fin_mkdirat(fd, path, mode);
//
//    return status;
//}
