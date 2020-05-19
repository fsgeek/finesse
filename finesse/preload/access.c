

#if 0

  #include <unistd.h>

       int access(const char *pathname, int mode);

       #include <fcntl.h>           /* Definition of AT_* constants */
       #include <unistd.h>

       int faccessat(int dirfd, const char *pathname, int mode, int flags);
#endif // 0
