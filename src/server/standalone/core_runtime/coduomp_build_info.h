#ifndef CODUOMP_BUILD_INFO_H
#define CODUOMP_BUILD_INFO_H

/* NOT_FROM_ORIGINAL_SOURCE: build identity for the recovered dedicated
 * engine's startup log. The original binary printed its retail build date and
 * had no revision. CODUOMP_GIT_REVISION is supplied by build-mk/server.mk from
 * `git describe`; a source tree without git metadata reports "unknown". The
 * date and time describe the compilation of the translation unit that reports
 * them, matching the client's CODUOMP_DISPLAY_BUILD_DATE and
 * CODUOMP_DISPLAY_BUILD_TIME. */
#ifndef CODUOMP_GIT_REVISION
#define CODUOMP_GIT_REVISION "unknown"
#endif

#define CODUOMP_BUILD_DATE __DATE__
#define CODUOMP_BUILD_TIME __TIME__

#endif
