/* dk_ver.h - macros and definitions for memory management */

/*
 *  Copyright © 2000-2001 Atheros Communications, Inc.,  All Rights Reserved.
 */

#ident  "ACI $Id: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/mdk/dk_ver.h#5 $, $Header: //depot/sw/releases/olca2.1-RC/host/tools/systemtools/mdk/dk_ver.h#5 $"

#ifndef __INCdk_verh
#define __INCdk_verh
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ART_VERSION_MAJOR 2
#define ART_VERSION_MINOR 3
#define ART_BUILD_NUM     7


#define MAUIDK_VER1 ("\n   --- Atheros: MDK (multi-device version) ---\n")

#define MAUIDK_VER_SUB ("              - Revision 2.3.7 ")
#ifdef CUSTOMER_REL
#define MAUIDK_VER2  ("              - Revision 2.3 BUILD #7 v53_mercury")
#ifdef ANWI
#define MAUIDK_VER3 ("\n            - Customer Version (ANWI BUILD)-\n")
#else
#define MAUIDK_VER3 ("\n            - Customer Version -\n")
#endif //ANWI
#else
#define MAUIDK_VER2 ("              - Revision 2.3 BUILD #7 v53_mercury")
#define MAUIDK_VER3 ("\n      --- Atheros INTERNAL USE ONLY ---\n")
#endif
#define DEVLIB_VER1 ("Devlib 6000 Revision 2.3 BUILD #7 v53_mercury\n")

#ifdef ART
#define MDK_CLIENT_VER1 ("\n   --- Atheros: ART Client (multi-device version) ---\n")
#else
#define MDK_CLIENT_VER1 ("\n   --- Atheros: MDK Client (multi-device version) ---\n")
#endif
#define MDK_CLIENT_VER2 ("         - Revision 2.3 BUILD #7 v53_mercury -\n")

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __INCdk_verh */
