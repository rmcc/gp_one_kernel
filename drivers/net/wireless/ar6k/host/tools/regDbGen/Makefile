all: regDomainGenDb
	./regDomainGenDb

regDomainGenDb: ../../../include/regulatory/reg_dbschema.h ../../../include/regulatory/reg_dbvalues.h
	gcc -Wall -g -I../../include -I../../../include regDomainGenDb.c -o regDomainGenDb

clean:
	rm regDomainGenDb *.bin regulatoryData_*.h
