Codetelligence Embedded SDIO Stack (PDK and HDK) Add-On Pack Read Me.

An SDIO kit may be delivered with an Add-On pack to support certain 
host controllers, operating system distributions or custom peripherals.
The Add-On pack is delivered as a zipped archive file containing an overlay for 
a PDK or HDK install. Add-on packs are specific to a PDK/HDK revision level.

The Add-On pack archive file has the following name format:
  add_on_pack.xxxxx.EMSDIO_M_N.yyyyy.tar.gz
  
  Where:
        xxxxx - is a description of the pack
        M & N - are the revision numbers for the PDK/HDK kit for this pack.
        yyyyy - is an informational build number.
        
          
Follow these steps to install an Add-On pack:

1. You must first install the SDIO PDK or HDK kit. 
2. Copy the Add-On pack (*.tar.gz file) to the base of the install directory i.e:
   "Codetelligence/HDK/"  or "Codetelligence/PDK/"
3. Expand/untar the zip file into this directory. 
   Example: "tar -xzf add_on_pack.xxxxx.EMSDIO_M_N.yyyyy.tar.gz"
 
        
For any issues concerning the Add-On pack, please contact tech support at:

support@codetelligence.com

