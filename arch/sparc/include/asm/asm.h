#ifndef _ASM_SPARC_ASM_H
#define _ASM_SPARC_ASM_H

#ifdef __ASSEMBLY__
# define __ASM_FORM(x)	x
#else
# define __ASM_FORM(x)	" " #x " "
#endif

#ifdef CONFIG_SPARC32
# define __ASM_SEL(a,b)	__ASM_FORM(a)
#else
# define __ASM_SEL(a,b)	__ASM_FORM(b)
#endif

#define _ASM_PTR	__ASM_SEL(.word, .xword)
#define _ASM_UAPTR	__ASM_SEL(.uaword, .uaxword)

#endif /* _ASM_SPARC_ASM_H */
