all: regDomainGenDb
	./regDomainGenDb
	mv regulatoryData_AG.bin regulatoryData_AG.diff
	mv regulatoryData_G.bin regulatoryData_G.diff

regDomainGenDb: ../../../include/regulatory/reg_dbschema.h ../../../include/regulatory/reg_dbvalues.h
	ln -s ../../tools/regDbGen/regDomainGenDb.c
	gcc -Wall -g -I../../include -I../../../include regDomainGenDb.c -o regDomainGenDb

clean:
	rm -f regDomainGenDb *.diff regulatoryData_*.h regDomainGenDb.c
