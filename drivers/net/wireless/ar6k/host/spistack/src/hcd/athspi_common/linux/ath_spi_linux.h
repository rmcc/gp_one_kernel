
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: ath_spi_linux.h

@abstract: common include file for OS-specific apis
 
@notice: Copyright (c), 2006-2007 Atheros Communications Inc.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef ATH_SPI_LINUX_H_
#define ATH_SPI_LINUX_H_

/* map the current DMA request buffer to a scatterlist array, returns 0 if request cannot be
 * mapped otherwise the number of entries used in the scatterlist is returned */
int HcdMapCurrentRequestBuffer(PSDHCD_DEVICE pDevice, struct scatterlist *pSGList, int MaxEntries);

#endif /*ATH_SPI_LINUX_H_*/
