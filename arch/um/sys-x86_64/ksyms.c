#include <linux/module.h>
#include <asm/string.h>
#include <asm/checksum.h>

/*XXX: we need them because they would be exported by x86_64 */
EXPORT_SYMBOL(__memcpy);
EXPORT_SYMBOL(csum_partial);
