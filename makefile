# CLIPS 6.4.1
# Console and Library
# macOS 13.3.1
# Ubuntu 22.04 LTS 
# Debian 11.3 
# Fedora 36 
# CentOS 9 
# Mint 20.3 

# The GNU Make Manual
# http://www.gnu.org/software/make/manual/make.html

# https://felixcrux.com/blog/creating-basic-makefile

# platform detection
PLATFORM = $(shell uname -s)

ifeq ($(PLATFORM),Darwin) # macOS
	CLIPS_OS = DARWIN
	WARNINGS = -Wall -Wundef -Wpointer-arith -Wshadow -Wstrict-aliasing \
	           -Winline -Wmissing-declarations -Wredundant-decls \
	           -Wmissing-prototypes -Wnested-externs -Wstrict-prototypes \
	           -Waggregate-return -Wno-implicit
endif

ifeq ($(PLATFORM),Linux) # linux
	CLIPS_OS = LINUX
	WARNINGS = -Wall -Wundef -Wpointer-arith -Wshadow -Wstrict-aliasing \
               -Winline -Wredundant-decls -Waggregate-return
endif
	    
OBJS = agenda.o analysis.o argacces.o bload.o bmathfun.o bsave.o \
 	classcom.o classexm.o classfun.o classinf.o classini.o \
 	classpsr.o clsltpsr.o commline.o conscomp.o constrct.o \
 	constrnt.o crstrtgy.o cstrcbin.o cstrccom.o cstrcpsr.o \
 	cstrnbin.o cstrnchk.o cstrncmp.o cstrnops.o cstrnpsr.o \
 	cstrnutl.o default.o defins.o developr.o dffctbin.o dffctbsc.o \
 	dffctcmp.o dffctdef.o dffctpsr.o dffnxbin.o dffnxcmp.o dffnxexe.o \
 	dffnxfun.o dffnxpsr.o dfinsbin.o dfinscmp.o drive.o emathfun.o \
 	engine.o envrnmnt.o envrnbld.o evaluatn.o expressn.o exprnbin.o \
 	exprnops.o exprnpsr.o extnfunc.o factbin.o factbld.o factcmp.o \
 	factcom.o factfun.o factgen.o facthsh.o factfile.o factlhs.o factmch.o \
 	factmngr.o factprt.o factqpsr.o factqury.o factrete.o factrhs.o \
 	filecom.o filertr.o fileutil.o generate.o genrcbin.o genrccmp.o \
 	genrccom.o genrcexe.o genrcfun.o genrcpsr.o globlbin.o globlbsc.o \
 	globlcmp.o globlcom.o globldef.o globlpsr.o immthpsr.o incrrset.o \
 	inherpsr.o inscom.o insfile.o insfun.o insmngr.o insmoddp.o \
 	insmult.o inspsr.o insquery.o insqypsr.o iofun.o lgcldpnd.o \
 	memalloc.o miscfun.o modulbin.o modulbsc.o modulcmp.o moduldef.o \
 	modulpsr.o modulutl.o msgcom.o msgfun.o msgpass.o msgpsr.o \
 	multifld.o multifun.o objbin.o objcmp.o objrtbin.o objrtbld.o \
 	objrtcmp.o objrtfnx.o objrtgen.o objrtmch.o parsefun.o pattern.o \
 	pprint.o prccode.o prcdrfun.o prcdrpsr.o prdctfun.o prntutil.o \
 	proflfun.o reorder.o reteutil.o retract.o router.o rulebin.o \
 	rulebld.o rulebsc.o rulecmp.o rulecom.o rulecstr.o ruledef.o \
 	ruledlt.o rulelhs.o rulepsr.o scanner.o sortfun.o strngfun.o \
 	strngrtr.o symblbin.o symblcmp.o symbol.o sysdep.o textpro.o \
 	tmpltbin.o tmpltbsc.o tmpltcmp.o tmpltdef.o tmpltfun.o tmpltlhs.o \
 	tmpltpsr.o tmpltrhs.o tmpltutl.o userdata.o userfunctions.o \
 	utility.o watch.o

all: release

debug : CC = gcc
debug : CFLAGS = -std=c99 -O0 -g
debug : LDLIBS = -lm
debug : clips

release : CC = gcc
release : CFLAGS = -std=c99 -O3 -fno-strict-aliasing
release : LDLIBS = -lm
release : clips

debug_cpp : CC = g++
debug_cpp : CFLAGS = -x c++ -std=c++11 -O0 -g
ifeq ($(PLATFORM),Darwin) # macOS
debug_cpp : WARNINGS += -Wcast-qual
endif
debug_cpp : LDLIBS = -lstdc++
debug_cpp : clips

release_cpp : CC = g++
release_cpp : CFLAGS = -x c++ -std=c++11 -O3 -fno-strict-aliasing
ifeq ($(PLATFORM),Darwin) # macOS
release_cpp : WARNINGS += -Wcast-qual
endif
release_cpp : LDLIBS = -lstdc++
release_cpp : clips

.c.o :
	$(CC) -c -D$(CLIPS_OS) $(CFLAGS) $(WARNINGS) $<

clips : main.o libclips.a
	$(CC) -o clips main.o -L. -lclips $(LDLIBS)
	
libclips.a : $(OBJS)
	rm -f $@   
	ar cq $@ $(OBJS)   

clean : 
	-rm -f main.o $(OBJS)
	-rm -f clips libclips.a
	
.PHONY : all clips clean debug release debug_cpp release_cpp

# Dependencies generated using "gcc -MM *.c"

agenda.o: agenda.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h crstrtgy.h agenda.h ruledef.h network.h match.h \
  conscomp.h extnfunc.h symbol.h symblcmp.h constrnt.h cstrccom.h \
  engine.h lgcldpnd.h retract.h memalloc.h modulutl.h scanner.h \
  multifld.h prntutil.h reteutil.h router.h rulebsc.h strngrtr.h \
  sysdep.h watch.h

analysis.o: analysis.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h cstrnchk.h constrnt.h evaluatn.h cstrnutl.h cstrnops.h \
  exprnpsr.h extnfunc.h expressn.h exprnops.h constrct.h userdata.h \
  moduldef.h utility.h symbol.h scanner.h generate.h analysis.h \
  reorder.h pattern.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h symblcmp.h cstrccom.h memalloc.h modulutl.h prntutil.h \
  router.h rulecstr.h rulepsr.h watch.h

argacces.o: argacces.c setup.h envrnmnt.h entities.h usrsetup.h \
  cstrnchk.h constrnt.h evaluatn.h constant.h extnfunc.h expressn.h \
  exprnops.h constrct.h userdata.h moduldef.h utility.h symbol.h \
  factmngr.h conscomp.h symblcmp.h tmpltdef.h factbld.h network.h \
  match.h ruledef.h agenda.h crstrtgy.h cstrccom.h facthsh.h insfun.h \
  object.h multifld.h objrtmch.h prntutil.h router.h sysdep.h argacces.h

bload.o: bload.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h bsave.h cstrnbin.h constrnt.h exprnpsr.h \
  extnfunc.h symbol.h scanner.h memalloc.h prntutil.h router.h bload.h \
  exprnbin.h sysdep.h symblbin.h

bmathfun.o: bmathfun.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h exprnpsr.h extnfunc.h symbol.h \
  scanner.h prntutil.h router.h bmathfun.h

bsave.o: bsave.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h bload.h extnfunc.h symbol.h exprnbin.h sysdep.h \
  symblbin.h cstrnbin.h constrnt.h exprnpsr.h scanner.h memalloc.h \
  prntutil.h router.h bsave.h

classcom.o: classcom.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h argacces.h classfun.h object.h constrnt.h multifld.h \
  match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  cstrccom.h objrtmch.h scanner.h classcom.h classini.h modulutl.h \
  msgcom.h msgpass.h prntutil.h router.h

classexm.o: classexm.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h classcom.h cstrccom.h object.h \
  constrnt.h multifld.h symbol.h match.h network.h ruledef.h agenda.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h \
  scanner.h classini.h insfun.h memalloc.h msgcom.h msgpass.h msgfun.h \
  prntutil.h router.h strngrtr.h sysdep.h classexm.h

classfun.o: classfun.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h classcom.h cstrccom.h object.h constrnt.h multifld.h \
  match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  objrtmch.h classini.h cstrcpsr.h strngfun.h inscom.h insfun.h \
  insmngr.h memalloc.h modulutl.h scanner.h msgfun.h msgpass.h \
  prntutil.h router.h classfun.h

classinf.o: classinf.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h classcom.h cstrccom.h object.h \
  constrnt.h multifld.h symbol.h match.h network.h ruledef.h agenda.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h objrtmch.h classexm.h \
  classfun.h scanner.h classini.h memalloc.h insfun.h msgcom.h msgpass.h \
  msgfun.h prntutil.h classinf.h

classini.o: classini.c setup.h envrnmnt.h entities.h usrsetup.h \
  classcom.h cstrccom.h moduldef.h userdata.h utility.h evaluatn.h \
  constant.h constrct.h object.h constrnt.h expressn.h exprnops.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classexm.h classfun.h \
  scanner.h classinf.h classpsr.h cstrcpsr.h strngfun.h inscom.h \
  insfun.h memalloc.h modulpsr.h modulutl.h msgcom.h msgpass.h watch.h \
  defins.h insquery.h bload.h exprnbin.h sysdep.h symblbin.h objbin.h \
  objcmp.h objrtbld.h objrtfnx.h classini.h

classpsr.o: classpsr.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h classcom.h cstrccom.h object.h constrnt.h multifld.h \
  match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  objrtmch.h classfun.h scanner.h clsltpsr.h cstrcpsr.h strngfun.h \
  inherpsr.h memalloc.h modulpsr.h modulutl.h msgpsr.h pprint.h \
  prntutil.h router.h classpsr.h

clsltpsr.o: clsltpsr.c setup.h envrnmnt.h entities.h usrsetup.h \
  classcom.h cstrccom.h moduldef.h userdata.h utility.h evaluatn.h \
  constant.h constrct.h object.h constrnt.h expressn.h exprnops.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h \
  cstrnchk.h cstrnpsr.h cstrnutl.h default.h insfun.h memalloc.h \
  pprint.h prntutil.h router.h clsltpsr.h

commline.o: commline.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h argacces.h expressn.h exprnops.h constrct.h userdata.h \
  moduldef.h utility.h evaluatn.h cstrcpsr.h strngfun.h exprnpsr.h \
  extnfunc.h symbol.h scanner.h fileutil.h memalloc.h multifld.h \
  pprint.h prcdrfun.h prcdrpsr.h constrnt.h prntutil.h router.h \
  strngrtr.h sysdep.h commline.h

conscomp.o: conscomp.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h cstrccom.h cstrncmp.h constrnt.h \
  exprnpsr.h extnfunc.h symbol.h scanner.h memalloc.h modulcmp.h \
  prntutil.h router.h sysdep.h network.h match.h ruledef.h agenda.h \
  crstrtgy.h conscomp.h symblcmp.h dffnxcmp.h dffnxfun.h tmpltcmp.h \
  tmpltdef.h factbld.h globlcmp.h globldef.h genrccmp.h genrcfun.h \
  objcmp.h object.h multifld.h objrtmch.h

constrct.o: constrct.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h commline.h cstrcpsr.h strngfun.h \
  exprnpsr.h extnfunc.h symbol.h scanner.h memalloc.h miscfun.h \
  modulutl.h multifld.h prcdrfun.h prcdrpsr.h constrnt.h prntutil.h \
  router.h ruledef.h network.h match.h agenda.h crstrtgy.h conscomp.h \
  symblcmp.h cstrccom.h sysdep.h watch.h

constrnt.o: constrnt.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h extnfunc.h symbol.h memalloc.h \
  multifld.h router.h scanner.h constrnt.h

crstrtgy.o: crstrtgy.c setup.h envrnmnt.h entities.h usrsetup.h agenda.h \
  ruledef.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h expressn.h exprnops.h network.h match.h conscomp.h \
  extnfunc.h symbol.h symblcmp.h constrnt.h cstrccom.h crstrtgy.h \
  argacces.h memalloc.h pattern.h scanner.h reorder.h reteutil.h

cstrcbin.o: cstrcbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h cstrcbin.h

cstrccom.o: cstrccom.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h extnfunc.h evaluatn.h expressn.h exprnops.h constrct.h \
  userdata.h moduldef.h utility.h symbol.h memalloc.h argacces.h \
  multifld.h modulutl.h scanner.h prntutil.h router.h commline.h \
  sysdep.h bload.h exprnbin.h symblbin.h cstrcpsr.h strngfun.h \
  cstrccom.h

cstrcpsr.o: cstrcpsr.c setup.h envrnmnt.h entities.h usrsetup.h router.h \
  watch.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h prcdrpsr.h constrnt.h exprnpsr.h \
  extnfunc.h symbol.h scanner.h memalloc.h modulutl.h modulpsr.h \
  pprint.h prntutil.h strngrtr.h sysdep.h cstrcpsr.h strngfun.h

cstrnbin.o: cstrnbin.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h memalloc.h prntutil.h router.h bload.h utility.h evaluatn.h \
  moduldef.h userdata.h extnfunc.h expressn.h exprnops.h constrct.h \
  symbol.h exprnbin.h sysdep.h symblbin.h bsave.h cstrnbin.h constrnt.h

cstrnchk.o: cstrnchk.c setup.h envrnmnt.h entities.h usrsetup.h \
  cstrnutl.h constrnt.h evaluatn.h constant.h extnfunc.h expressn.h \
  exprnops.h constrct.h userdata.h moduldef.h utility.h symbol.h \
  multifld.h prntutil.h router.h classcom.h cstrccom.h object.h match.h \
  network.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  objrtmch.h classexm.h inscom.h insfun.h cstrnchk.h

cstrncmp.o: cstrncmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h conscomp.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  memalloc.h prntutil.h router.h sysdep.h cstrncmp.h constrnt.h

cstrnops.o: cstrnops.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h constrnt.h evaluatn.h cstrnchk.h cstrnutl.h extnfunc.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  symbol.h memalloc.h multifld.h router.h scanner.h cstrnops.h

cstrnpsr.o: cstrnpsr.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h cstrnchk.h constrnt.h evaluatn.h cstrnutl.h expressn.h \
  exprnops.h constrct.h userdata.h moduldef.h utility.h memalloc.h \
  pprint.h prntutil.h router.h scanner.h sysdep.h cstrnpsr.h

cstrnutl.o: cstrnutl.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h extnfunc.h symbol.h memalloc.h \
  multifld.h router.h scanner.h cstrnutl.h constrnt.h

default.o: default.c setup.h envrnmnt.h entities.h usrsetup.h constant.h \
  constrnt.h evaluatn.h cstrnchk.h cstrnutl.h exprnpsr.h extnfunc.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  symbol.h scanner.h factmngr.h conscomp.h symblcmp.h tmpltdef.h \
  factbld.h network.h match.h ruledef.h agenda.h crstrtgy.h cstrccom.h \
  facthsh.h inscom.h insfun.h object.h multifld.h objrtmch.h pprint.h \
  prntutil.h router.h default.h

defins.o: defins.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h dfinsbin.h defins.h conscomp.h symblcmp.h cstrccom.h \
  object.h constrnt.h multifld.h match.h network.h ruledef.h agenda.h \
  crstrtgy.h objrtmch.h dfinscmp.h argacces.h classcom.h classfun.h \
  scanner.h cstrcpsr.h strngfun.h insfun.h inspsr.h memalloc.h \
  modulpsr.h modulutl.h pprint.h prntutil.h router.h

developr.o: developr.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h extnfunc.h symbol.h factmngr.h \
  conscomp.h symblcmp.h tmpltdef.h constrnt.h factbld.h network.h \
  match.h ruledef.h agenda.h crstrtgy.h cstrccom.h facthsh.h inscom.h \
  insfun.h object.h multifld.h objrtmch.h modulutl.h scanner.h \
  prntutil.h router.h classcom.h classfun.h developr.h

dffctbin.o: dffctbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h dffctdef.h conscomp.h symblcmp.h cstrccom.h \
  memalloc.h dffctbin.h cstrcbin.h modulbin.h

dffctbsc.o: dffctbsc.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h cstrccom.h cstrcpsr.h strngfun.h \
  dffctdef.h conscomp.h extnfunc.h symbol.h symblcmp.h dffctpsr.h \
  factrhs.h factmngr.h tmpltdef.h constrnt.h factbld.h network.h match.h \
  ruledef.h agenda.h crstrtgy.h facthsh.h scanner.h memalloc.h \
  multifld.h router.h dffctbin.h cstrcbin.h modulbin.h dffctcmp.h \
  dffctbsc.h

dffctcmp.o: dffctcmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  conscomp.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  dffctdef.h cstrccom.h dffctcmp.h

dffctdef.o: dffctdef.c setup.h envrnmnt.h entities.h usrsetup.h \
  dffctbsc.h dffctdef.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h conscomp.h extnfunc.h expressn.h exprnops.h \
  symbol.h symblcmp.h cstrccom.h dffctpsr.h memalloc.h bload.h \
  exprnbin.h sysdep.h symblbin.h dffctbin.h cstrcbin.h modulbin.h \
  dffctcmp.h

dffctpsr.o: dffctpsr.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h cstrcpsr.h strngfun.h dffctbsc.h dffctdef.h conscomp.h \
  symblcmp.h cstrccom.h factrhs.h factmngr.h tmpltdef.h constrnt.h \
  factbld.h network.h match.h ruledef.h agenda.h crstrtgy.h facthsh.h \
  scanner.h memalloc.h modulutl.h pprint.h prntutil.h router.h \
  dffctpsr.h

dffnxbin.o: dffnxbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h cstrcbin.h cstrccom.h memalloc.h modulbin.h \
  dffnxbin.h dffnxfun.h

dffnxcmp.o: dffnxcmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  conscomp.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  dffnxcmp.h dffnxfun.h

dffnxexe.o: dffnxexe.c setup.h envrnmnt.h entities.h usrsetup.h \
  constrct.h userdata.h moduldef.h utility.h evaluatn.h constant.h \
  prcdrfun.h prccode.h expressn.h exprnops.h scanner.h symbol.h \
  prntutil.h proflfun.h router.h watch.h dffnxexe.h dffnxfun.h

dffnxfun.o: dffnxfun.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h dffnxbin.h dffnxfun.h dffnxcmp.h cstrcpsr.h strngfun.h \
  dffnxpsr.h modulpsr.h scanner.h dffnxexe.h watch.h argacces.h \
  cstrccom.h memalloc.h modulutl.h multifld.h prntutil.h router.h

dffnxpsr.o: dffnxpsr.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h network.h match.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  symblcmp.h constrnt.h cstrccom.h genrccom.h genrcfun.h cstrcpsr.h \
  strngfun.h dffnxfun.h exprnpsr.h scanner.h memalloc.h modulutl.h \
  pprint.h prccode.h prntutil.h router.h dffnxpsr.h

dfinsbin.o: dfinsbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h cstrcbin.h defins.h conscomp.h symblcmp.h \
  cstrccom.h object.h constrnt.h multifld.h match.h network.h ruledef.h \
  agenda.h crstrtgy.h objrtmch.h memalloc.h modulbin.h dfinsbin.h

dfinscmp.o: dfinscmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  conscomp.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  defins.h cstrccom.h object.h constrnt.h multifld.h match.h network.h \
  ruledef.h agenda.h crstrtgy.h objrtmch.h dfinscmp.h

drive.o: drive.c setup.h envrnmnt.h entities.h usrsetup.h agenda.h \
  ruledef.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h expressn.h exprnops.h network.h match.h conscomp.h \
  extnfunc.h symbol.h symblcmp.h constrnt.h cstrccom.h crstrtgy.h \
  engine.h lgcldpnd.h retract.h incrrset.h memalloc.h prntutil.h \
  reteutil.h router.h drive.h

emathfun.o: emathfun.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h extnfunc.h symbol.h miscfun.h \
  prntutil.h router.h emathfun.h

engine.o: engine.c setup.h envrnmnt.h entities.h usrsetup.h agenda.h \
  ruledef.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h expressn.h exprnops.h network.h match.h conscomp.h \
  extnfunc.h symbol.h symblcmp.h constrnt.h cstrccom.h crstrtgy.h \
  argacces.h commline.h factmngr.h tmpltdef.h factbld.h facthsh.h \
  inscom.h insfun.h object.h multifld.h objrtmch.h memalloc.h modulutl.h \
  scanner.h prccode.h prcdrfun.h prntutil.h proflfun.h reteutil.h \
  retract.h router.h ruledlt.h sysdep.h watch.h engine.h lgcldpnd.h

envrnbld.o: envrnbld.c setup.h envrnmnt.h entities.h usrsetup.h \
  bmathfun.h evaluatn.h constant.h commline.h emathfun.h engine.h \
  lgcldpnd.h match.h network.h ruledef.h constrct.h userdata.h \
  moduldef.h utility.h expressn.h exprnops.h agenda.h symbol.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  retract.h filecom.h iofun.h memalloc.h miscfun.h multifun.h parsefun.h \
  pprint.h prccode.h scanner.h prcdrfun.h prdctfun.h prntutil.h \
  proflfun.h router.h sortfun.h strngfun.h sysdep.h textpro.h watch.h \
  dffctdef.h genrccom.h genrcfun.h dffnxfun.h globldef.h tmpltdef.h \
  factbld.h classini.h object.h multifld.h objrtmch.h envrnbld.h

envrnmnt.o: envrnmnt.c setup.h envrnmnt.h entities.h usrsetup.h \
  bmathfun.h evaluatn.h constant.h commline.h emathfun.h engine.h \
  lgcldpnd.h match.h network.h ruledef.h constrct.h userdata.h \
  moduldef.h utility.h expressn.h exprnops.h agenda.h symbol.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  retract.h filecom.h iofun.h memalloc.h miscfun.h multifun.h parsefun.h \
  prccode.h scanner.h prcdrfun.h prdctfun.h prntutil.h proflfun.h \
  router.h sortfun.h strngfun.h sysdep.h textpro.h watch.h dffctdef.h \
  genrccom.h genrcfun.h dffnxfun.h globldef.h tmpltdef.h factbld.h \
  classini.h object.h multifld.h objrtmch.h

evaluatn.o: evaluatn.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h commline.h factmngr.h conscomp.h \
  extnfunc.h symbol.h symblcmp.h tmpltdef.h constrnt.h factbld.h \
  network.h match.h ruledef.h agenda.h crstrtgy.h cstrccom.h facthsh.h \
  memalloc.h modulutl.h scanner.h router.h prcdrfun.h multifld.h \
  prntutil.h exprnpsr.h proflfun.h sysdep.h dffnxfun.h genrccom.h \
  genrcfun.h object.h objrtmch.h inscom.h insfun.h

expressn.o: expressn.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h memalloc.h prntutil.h router.h

exprnbin.o: exprnbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h dffctdef.h conscomp.h symblcmp.h cstrccom.h \
  memalloc.h network.h match.h ruledef.h agenda.h crstrtgy.h constrnt.h \
  genrcbin.h genrcfun.h dffnxbin.h dffnxfun.h factmngr.h tmpltdef.h \
  factbld.h facthsh.h tmpltbin.h cstrcbin.h modulbin.h globlbin.h \
  globldef.h objbin.h object.h multifld.h objrtmch.h insfun.h inscom.h

exprnops.o: exprnops.c setup.h envrnmnt.h entities.h usrsetup.h \
  cstrnchk.h constrnt.h evaluatn.h constant.h cstrnops.h cstrnutl.h \
  extnfunc.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h symbol.h memalloc.h prntutil.h router.h

exprnpsr.o: exprnpsr.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h cstrnchk.h constrnt.h memalloc.h \
  modulutl.h symbol.h scanner.h pprint.h prcdrfun.h prntutil.h router.h \
  strngrtr.h network.h match.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  extnfunc.h symblcmp.h cstrccom.h genrccom.h genrcfun.h dffnxfun.h \
  exprnpsr.h

extnfunc.o: extnfunc.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h exprnpsr.h extnfunc.h symbol.h \
  scanner.h factmngr.h conscomp.h symblcmp.h tmpltdef.h constrnt.h \
  factbld.h network.h match.h ruledef.h agenda.h crstrtgy.h cstrccom.h \
  facthsh.h memalloc.h router.h inscom.h insfun.h object.h multifld.h \
  objrtmch.h

factbin.o: factbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h factmngr.h conscomp.h symblcmp.h tmpltdef.h \
  constrnt.h factbld.h network.h match.h ruledef.h agenda.h crstrtgy.h \
  cstrccom.h facthsh.h memalloc.h pattern.h scanner.h reorder.h \
  reteutil.h rulebin.h cstrcbin.h modulbin.h factbin.h

factbld.o: factbld.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h factcmp.h network.h match.h ruledef.h agenda.h \
  symbol.h crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h \
  cstrccom.h pattern.h scanner.h reorder.h factgen.h factlhs.h factmch.h \
  factbld.h factmngr.h tmpltdef.h facthsh.h memalloc.h modulutl.h \
  reteutil.h router.h

factcmp.o: factcmp.c setup.h envrnmnt.h entities.h usrsetup.h factbld.h \
  network.h match.h ruledef.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h expressn.h exprnops.h agenda.h symbol.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  factmngr.h tmpltdef.h facthsh.h factcmp.h pattern.h scanner.h \
  reorder.h

factcom.o: factcom.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h extnfunc.h symbol.h factmngr.h conscomp.h \
  symblcmp.h tmpltdef.h constrnt.h factbld.h network.h match.h ruledef.h \
  agenda.h crstrtgy.h cstrccom.h facthsh.h factrhs.h scanner.h \
  multifld.h pprint.h prntutil.h router.h sysdep.h tmpltutl.h factcom.h

factfile.o: factfile.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h bload.h extnfunc.h symbol.h exprnbin.h \
  sysdep.h symblbin.h cstrcpsr.h strngfun.h factmngr.h conscomp.h \
  symblcmp.h tmpltdef.h constrnt.h factbld.h network.h match.h ruledef.h \
  agenda.h crstrtgy.h cstrccom.h facthsh.h factrhs.h scanner.h insmngr.h \
  object.h multifld.h objrtmch.h inscom.h insfun.h memalloc.h modulpsr.h \
  modulutl.h prntutil.h router.h strngrtr.h tmpltutl.h factfile.h

factfun.o: factfun.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h extnfunc.h symbol.h multifld.h prntutil.h \
  router.h sysdep.h tmpltutl.h constrnt.h factmngr.h conscomp.h \
  symblcmp.h tmpltdef.h factbld.h network.h match.h ruledef.h agenda.h \
  crstrtgy.h cstrccom.h facthsh.h factfun.h

factgen.o: factgen.c setup.h envrnmnt.h entities.h usrsetup.h constant.h \
  constrct.h userdata.h moduldef.h utility.h evaluatn.h exprnpsr.h \
  extnfunc.h expressn.h exprnops.h symbol.h scanner.h factmch.h \
  factbld.h network.h match.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  symblcmp.h constrnt.h cstrccom.h factmngr.h tmpltdef.h facthsh.h \
  factprt.h factrete.h memalloc.h pattern.h reorder.h prcdrpsr.h \
  reteutil.h router.h sysdep.h tmpltfun.h tmpltlhs.h tmpltutl.h \
  factgen.h

facthsh.o: facthsh.c setup.h envrnmnt.h entities.h usrsetup.h constant.h \
  factmngr.h conscomp.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  tmpltdef.h constrnt.h factbld.h network.h match.h ruledef.h agenda.h \
  crstrtgy.h cstrccom.h facthsh.h memalloc.h multifld.h router.h \
  sysdep.h lgcldpnd.h

factlhs.o: factlhs.c setup.h envrnmnt.h entities.h usrsetup.h cstrcpsr.h \
  strngfun.h modulpsr.h evaluatn.h constant.h moduldef.h userdata.h \
  utility.h symbol.h scanner.h modulutl.h pattern.h expressn.h \
  exprnops.h constrct.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h reorder.h \
  pprint.h prntutil.h router.h tmpltdef.h factbld.h tmpltlhs.h \
  tmpltpsr.h tmpltutl.h factmngr.h facthsh.h factlhs.h

factmch.o: factmch.c setup.h envrnmnt.h entities.h usrsetup.h drive.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h match.h network.h ruledef.h agenda.h symbol.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  engine.h lgcldpnd.h retract.h factgen.h reorder.h pattern.h scanner.h \
  factrete.h incrrset.h memalloc.h prntutil.h reteutil.h router.h \
  sysdep.h tmpltdef.h factbld.h factmch.h factmngr.h facthsh.h

factmngr.o: factmngr.c setup.h envrnmnt.h entities.h usrsetup.h \
  commline.h default.h constrnt.h evaluatn.h constant.h engine.h \
  lgcldpnd.h match.h network.h ruledef.h constrct.h userdata.h \
  moduldef.h utility.h expressn.h exprnops.h agenda.h symbol.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h cstrccom.h retract.h \
  factbin.h factbld.h factcmp.h pattern.h scanner.h reorder.h factcom.h \
  factfile.h factfun.h factmngr.h tmpltdef.h facthsh.h factmch.h \
  factqury.h factrhs.h memalloc.h multifld.h prntutil.h router.h \
  strngrtr.h sysdep.h tmpltbsc.h tmpltfun.h tmpltutl.h watch.h \
  cstrnchk.h

factprt.o: factprt.c setup.h envrnmnt.h entities.h usrsetup.h factgen.h \
  reorder.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h pattern.h symbol.h scanner.h match.h \
  network.h ruledef.h agenda.h crstrtgy.h conscomp.h extnfunc.h \
  symblcmp.h constrnt.h cstrccom.h prntutil.h router.h factprt.h

factqpsr.o: factqpsr.c setup.h envrnmnt.h entities.h usrsetup.h \
  exprnpsr.h extnfunc.h evaluatn.h constant.h expressn.h exprnops.h \
  constrct.h userdata.h moduldef.h utility.h symbol.h scanner.h \
  factqury.h factmngr.h conscomp.h symblcmp.h tmpltdef.h constrnt.h \
  factbld.h network.h match.h ruledef.h agenda.h crstrtgy.h cstrccom.h \
  facthsh.h modulutl.h prcdrpsr.h pprint.h prntutil.h router.h \
  strngrtr.h factqpsr.h

factqury.o: factqury.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h memalloc.h exprnpsr.h extnfunc.h \
  symbol.h scanner.h modulutl.h tmpltutl.h constrnt.h factmngr.h \
  conscomp.h symblcmp.h tmpltdef.h factbld.h network.h match.h ruledef.h \
  agenda.h crstrtgy.h cstrccom.h facthsh.h insfun.h object.h multifld.h \
  objrtmch.h factqpsr.h prcdrfun.h prntutil.h router.h factqury.h

factrete.o: factrete.c setup.h envrnmnt.h entities.h usrsetup.h drive.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h match.h network.h ruledef.h agenda.h symbol.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  engine.h lgcldpnd.h retract.h factgen.h reorder.h pattern.h scanner.h \
  factmch.h factbld.h factmngr.h tmpltdef.h facthsh.h incrrset.h \
  memalloc.h multifld.h reteutil.h router.h factrete.h

factrhs.o: factrhs.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h cstrcpsr.h strngfun.h exprnpsr.h scanner.h modulutl.h \
  modulpsr.h pattern.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h symblcmp.h constrnt.h cstrccom.h reorder.h pprint.h \
  prntutil.h router.h strngrtr.h tmpltpsr.h tmpltdef.h factbld.h \
  tmpltrhs.h tmpltutl.h factmngr.h facthsh.h factrhs.h

filecom.o: filecom.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h bload.h extnfunc.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h commline.h cstrcpsr.h strngfun.h fileutil.h \
  memalloc.h router.h filecom.h

filertr.o: filertr.c setup.h envrnmnt.h entities.h usrsetup.h constant.h \
  memalloc.h router.h sysdep.h filertr.h

fileutil.o: fileutil.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h commline.h cstrcpsr.h strngfun.h \
  memalloc.h prcdrfun.h pprint.h prntutil.h router.h scanner.h \
  strngrtr.h sysdep.h filecom.h fileutil.h

generate.o: generate.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h exprnpsr.h extnfunc.h symbol.h \
  scanner.h globlpsr.h memalloc.h pattern.h match.h network.h ruledef.h \
  agenda.h crstrtgy.h conscomp.h symblcmp.h constrnt.h cstrccom.h \
  reorder.h prntutil.h router.h generate.h analysis.h

genrcbin.o: genrcbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h cstrcbin.h cstrccom.h genrccom.h genrcfun.h \
  conscomp.h symblcmp.h memalloc.h modulbin.h objbin.h object.h \
  constrnt.h multifld.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  objrtmch.h router.h genrcbin.h

genrccmp.o: genrccmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  conscomp.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  genrccom.h genrcfun.h network.h match.h ruledef.h agenda.h crstrtgy.h \
  constrnt.h cstrccom.h objcmp.h object.h multifld.h objrtmch.h \
  genrccmp.h

genrccom.o: genrccom.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h bload.h extnfunc.h symbol.h exprnbin.h \
  sysdep.h symblbin.h classcom.h cstrccom.h object.h constrnt.h \
  multifld.h match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  symblcmp.h objrtmch.h inscom.h insfun.h cstrcpsr.h strngfun.h \
  genrcbin.h genrcfun.h genrccmp.h genrcexe.h genrcpsr.h memalloc.h \
  modulpsr.h scanner.h modulutl.h router.h strngrtr.h watch.h prntutil.h \
  genrccom.h

genrcexe.o: genrcexe.c setup.h envrnmnt.h entities.h usrsetup.h \
  classcom.h cstrccom.h moduldef.h userdata.h utility.h evaluatn.h \
  constant.h constrct.h object.h constrnt.h expressn.h exprnops.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h \
  insfun.h argacces.h genrccom.h genrcfun.h prcdrfun.h prccode.h \
  prntutil.h proflfun.h router.h genrcexe.h

genrcfun.o: genrcfun.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h classcom.h cstrccom.h object.h constrnt.h multifld.h \
  match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  objrtmch.h classfun.h scanner.h argacces.h cstrcpsr.h strngfun.h \
  genrccom.h genrcfun.h genrcexe.h memalloc.h modulutl.h prccode.h \
  prntutil.h router.h

genrcpsr.o: genrcpsr.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h dffnxfun.h classfun.h object.h constrnt.h multifld.h \
  match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  cstrccom.h objrtmch.h scanner.h classcom.h cstrcpsr.h strngfun.h \
  exprnpsr.h genrccom.h genrcfun.h immthpsr.h memalloc.h modulutl.h \
  pprint.h prcdrpsr.h prccode.h prntutil.h router.h genrcpsr.h

globlbin.o: globlbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h globlbsc.h globldef.h conscomp.h symblcmp.h \
  cstrccom.h memalloc.h multifld.h globlbin.h modulbin.h cstrcbin.h

globlbsc.o: globlbsc.c setup.h envrnmnt.h entities.h usrsetup.h \
  constrct.h userdata.h moduldef.h utility.h evaluatn.h constant.h \
  extnfunc.h expressn.h exprnops.h symbol.h globlbin.h modulbin.h \
  cstrcbin.h globldef.h conscomp.h symblcmp.h cstrccom.h globlcmp.h \
  globlcom.h multifld.h watch.h globlbsc.h

globlcmp.o: globlcmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  conscomp.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  globldef.h cstrccom.h globlcmp.h

globlcom.o: globlcom.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h extnfunc.h symbol.h globldef.h \
  conscomp.h symblcmp.h cstrccom.h prntutil.h router.h globlcom.h

globldef.o: globldef.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h globlbin.h modulbin.h cstrcbin.h globldef.h conscomp.h \
  symblcmp.h cstrccom.h commline.h globlbsc.h globlcmp.h globlcom.h \
  globlpsr.h memalloc.h modulpsr.h scanner.h modulutl.h multifld.h \
  prntutil.h router.h strngrtr.h

globlpsr.o: globlpsr.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h cstrcpsr.h strngfun.h exprnpsr.h scanner.h globlbsc.h \
  globldef.h conscomp.h symblcmp.h cstrccom.h memalloc.h modulpsr.h \
  modulutl.h multifld.h pprint.h prntutil.h router.h watch.h globlpsr.h

immthpsr.o: immthpsr.c setup.h envrnmnt.h entities.h usrsetup.h \
  classcom.h cstrccom.h moduldef.h userdata.h utility.h evaluatn.h \
  constant.h constrct.h object.h constrnt.h expressn.h exprnops.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h \
  cstrnutl.h exprnpsr.h genrcpsr.h genrcfun.h memalloc.h prccode.h \
  immthpsr.h

incrrset.o: incrrset.c setup.h envrnmnt.h entities.h usrsetup.h agenda.h \
  ruledef.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h expressn.h exprnops.h network.h match.h conscomp.h \
  extnfunc.h symbol.h symblcmp.h constrnt.h cstrccom.h crstrtgy.h \
  argacces.h drive.h engine.h lgcldpnd.h retract.h pattern.h scanner.h \
  reorder.h router.h reteutil.h incrrset.h

inherpsr.o: inherpsr.c setup.h envrnmnt.h entities.h usrsetup.h \
  classcom.h cstrccom.h moduldef.h userdata.h utility.h evaluatn.h \
  constant.h constrct.h object.h constrnt.h expressn.h exprnops.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h \
  memalloc.h modulutl.h pprint.h prntutil.h router.h inherpsr.h

inscom.o: inscom.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h classcom.h cstrccom.h object.h constrnt.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h \
  classinf.h commline.h exprnpsr.h insfile.h insfun.h insmngr.h inscom.h \
  insmoddp.h insmult.h inspsr.h lgcldpnd.h memalloc.h msgcom.h msgpass.h \
  msgfun.h prntutil.h router.h strngrtr.h sysdep.h

insfile.o: insfile.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h classcom.h cstrccom.h object.h constrnt.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h \
  memalloc.h factmngr.h tmpltdef.h factbld.h facthsh.h inscom.h insfun.h \
  insmngr.h inspsr.h prntutil.h router.h strngrtr.h symblbin.h sysdep.h \
  insfile.h

insfun.o: insfun.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h classcom.h cstrccom.h object.h constrnt.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h \
  cstrnchk.h drive.h engine.h lgcldpnd.h retract.h inscom.h insfun.h \
  insmngr.h memalloc.h modulutl.h msgcom.h msgpass.h msgfun.h prccode.h \
  prntutil.h router.h

insmngr.o: insmngr.c setup.h envrnmnt.h entities.h usrsetup.h network.h \
  match.h ruledef.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h expressn.h exprnops.h agenda.h symbol.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  drive.h objrtmch.h object.h multifld.h lgcldpnd.h classcom.h \
  classfun.h scanner.h cstrnchk.h engine.h retract.h insfun.h memalloc.h \
  miscfun.h modulutl.h msgcom.h msgpass.h msgfun.h prccode.h prntutil.h \
  router.h sysdep.h insmngr.h inscom.h watch.h

insmoddp.o: insmoddp.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h extnfunc.h symbol.h inscom.h insfun.h \
  object.h constrnt.h multifld.h match.h network.h ruledef.h agenda.h \
  crstrtgy.h conscomp.h symblcmp.h cstrccom.h objrtmch.h insmngr.h \
  inspsr.h memalloc.h miscfun.h msgcom.h msgpass.h msgfun.h prccode.h \
  scanner.h prntutil.h router.h insmoddp.h

insmult.o: insmult.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h extnfunc.h symbol.h insfun.h object.h constrnt.h \
  multifld.h match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  symblcmp.h cstrccom.h objrtmch.h msgfun.h msgpass.h multifun.h \
  prntutil.h router.h insmult.h

inspsr.o: inspsr.c setup.h envrnmnt.h entities.h usrsetup.h classcom.h \
  cstrccom.h moduldef.h userdata.h utility.h evaluatn.h constant.h \
  constrct.h object.h constrnt.h expressn.h exprnops.h multifld.h \
  symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h classinf.h \
  exprnpsr.h pprint.h prntutil.h router.h inspsr.h

insquery.o: insquery.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h classcom.h cstrccom.h object.h \
  constrnt.h multifld.h symbol.h match.h network.h ruledef.h agenda.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h \
  scanner.h exprnpsr.h insfun.h insmngr.h inscom.h insqypsr.h memalloc.h \
  prcdrfun.h prntutil.h router.h insquery.h

insqypsr.o: insqypsr.c setup.h envrnmnt.h entities.h usrsetup.h \
  classcom.h cstrccom.h moduldef.h userdata.h utility.h evaluatn.h \
  constant.h constrct.h object.h constrnt.h expressn.h exprnops.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h exprnpsr.h scanner.h \
  insquery.h prcdrpsr.h pprint.h prntutil.h router.h strngrtr.h \
  insqypsr.h

iofun.o: iofun.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h commline.h extnfunc.h symbol.h filertr.h \
  memalloc.h miscfun.h prntutil.h router.h scanner.h strngrtr.h sysdep.h \
  iofun.h

lgcldpnd.o: lgcldpnd.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h engine.h lgcldpnd.h match.h network.h \
  ruledef.h agenda.h symbol.h crstrtgy.h conscomp.h extnfunc.h \
  symblcmp.h constrnt.h cstrccom.h retract.h factmngr.h tmpltdef.h \
  factbld.h facthsh.h memalloc.h pattern.h scanner.h reorder.h \
  reteutil.h router.h

main.o: main.c clips.h setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h memalloc.h cstrcpsr.h strngfun.h \
  fileutil.h envrnbld.h extnfunc.h symbol.h commline.h prntutil.h \
  router.h filertr.h strngrtr.h iofun.h sysdep.h bmathfun.h exprnpsr.h \
  scanner.h miscfun.h watch.h modulbsc.h bload.h exprnbin.h symblbin.h \
  bsave.h ruledef.h network.h match.h agenda.h crstrtgy.h conscomp.h \
  symblcmp.h constrnt.h cstrccom.h rulebsc.h engine.h lgcldpnd.h \
  retract.h drive.h incrrset.h rulecom.h dffctdef.h dffctbsc.h \
  tmpltdef.h factbld.h tmpltbsc.h tmpltfun.h factmngr.h facthsh.h \
  factcom.h factfun.h globldef.h globlbsc.h globlcom.h dffnxfun.h \
  genrccom.h genrcfun.h classcom.h object.h multifld.h objrtmch.h \
  classexm.h classfun.h classinf.h classini.h classpsr.h defins.h \
  inscom.h insfun.h insfile.h insmngr.h msgcom.h msgpass.h

memalloc.o: memalloc.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h memalloc.h prntutil.h router.h utility.h evaluatn.h \
  moduldef.h userdata.h

miscfun.o: miscfun.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h exprnpsr.h extnfunc.h symbol.h scanner.h \
  memalloc.h multifld.h prntutil.h router.h sysdep.h dffnxfun.h \
  factfun.h factmngr.h conscomp.h symblcmp.h tmpltdef.h constrnt.h \
  factbld.h network.h match.h ruledef.h agenda.h crstrtgy.h cstrccom.h \
  facthsh.h tmpltutl.h miscfun.h

modulbin.o: modulbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h cstrcbin.h memalloc.h modulbin.h

modulbsc.o: modulbsc.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h bload.h extnfunc.h symbol.h exprnbin.h \
  sysdep.h symblbin.h modulbin.h cstrcbin.h modulcmp.h multifld.h \
  prntutil.h router.h modulbsc.h

modulcmp.o: modulcmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  conscomp.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  sysdep.h modulcmp.h

moduldef.o: moduldef.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h bload.h extnfunc.h symbol.h exprnbin.h \
  sysdep.h symblbin.h modulbin.h cstrcbin.h memalloc.h modulbsc.h \
  modulcmp.h modulpsr.h scanner.h prntutil.h router.h

modulpsr.o: modulpsr.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h cstrcpsr.h strngfun.h extnfunc.h \
  symbol.h memalloc.h modulutl.h scanner.h pprint.h prntutil.h router.h \
  bload.h exprnbin.h sysdep.h symblbin.h modulpsr.h

modulutl.o: modulutl.c setup.h envrnmnt.h entities.h usrsetup.h \
  cstrcpsr.h strngfun.h memalloc.h modulpsr.h evaluatn.h constant.h \
  moduldef.h userdata.h utility.h symbol.h scanner.h pprint.h prntutil.h \
  router.h sysdep.h watch.h expressn.h exprnops.h constrct.h modulutl.h

msgcom.o: msgcom.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h bload.h extnfunc.h symbol.h exprnbin.h sysdep.h \
  symblbin.h classcom.h cstrccom.h object.h constrnt.h multifld.h \
  match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  objrtmch.h classfun.h scanner.h classinf.h msgpsr.h insfun.h \
  insmoddp.h msgfun.h msgpass.h memalloc.h prccode.h prntutil.h router.h \
  watch.h msgcom.h

msgfun.o: msgfun.c setup.h envrnmnt.h entities.h usrsetup.h classcom.h \
  cstrccom.h moduldef.h userdata.h utility.h evaluatn.h constant.h \
  constrct.h object.h constrnt.h expressn.h exprnops.h multifld.h \
  symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h inscom.h \
  insfun.h memalloc.h msgcom.h msgpass.h prccode.h prntutil.h router.h \
  msgfun.h

msgpass.o: msgpass.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h classcom.h cstrccom.h object.h constrnt.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h \
  commline.h exprnpsr.h inscom.h insfun.h memalloc.h msgcom.h msgpass.h \
  msgfun.h prccode.h prcdrfun.h prntutil.h proflfun.h router.h \
  strngfun.h

msgpsr.o: msgpsr.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h classcom.h cstrccom.h object.h constrnt.h multifld.h \
  match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  objrtmch.h classfun.h scanner.h cstrcpsr.h strngfun.h cstrnchk.h \
  exprnpsr.h insfun.h memalloc.h modulutl.h msgcom.h msgpass.h msgfun.h \
  pprint.h prccode.h prntutil.h router.h strngrtr.h msgpsr.h

multifld.o: multifld.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h evaluatn.h exprnops.h expressn.h constrct.h userdata.h \
  moduldef.h utility.h memalloc.h object.h constrnt.h multifld.h \
  symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  extnfunc.h symblcmp.h cstrccom.h objrtmch.h scanner.h prntutil.h \
  router.h strngrtr.h

multifun.o: multifun.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h exprnpsr.h extnfunc.h symbol.h \
  scanner.h memalloc.h multifld.h multifun.h object.h constrnt.h match.h \
  network.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  cstrccom.h objrtmch.h pprint.h prcdrpsr.h prcdrfun.h prntutil.h \
  router.h

objbin.o: objbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h classcom.h cstrccom.h object.h constrnt.h \
  multifld.h match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  symblcmp.h objrtmch.h classfun.h scanner.h classini.h cstrcbin.h \
  cstrnbin.h insfun.h memalloc.h modulbin.h msgcom.h msgpass.h msgfun.h \
  prntutil.h router.h objrtbin.h objbin.h

objcmp.o: objcmp.c setup.h envrnmnt.h entities.h usrsetup.h conscomp.h \
  constrct.h userdata.h moduldef.h utility.h evaluatn.h constant.h \
  extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h classcom.h \
  cstrccom.h object.h constrnt.h multifld.h match.h network.h ruledef.h \
  agenda.h crstrtgy.h objrtmch.h classfun.h scanner.h classini.h \
  cstrncmp.h objrtfnx.h sysdep.h objrtcmp.h objcmp.h

objrtbin.o: objrtbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h classfun.h object.h constrnt.h multifld.h match.h \
  network.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h \
  cstrccom.h objrtmch.h scanner.h classcom.h memalloc.h insfun.h \
  pattern.h reorder.h reteutil.h rulebin.h cstrcbin.h modulbin.h \
  objrtbin.h

objrtbld.o: objrtbld.c setup.h envrnmnt.h entities.h usrsetup.h \
  classcom.h cstrccom.h moduldef.h userdata.h utility.h evaluatn.h \
  constant.h constrct.h object.h constrnt.h expressn.h exprnops.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h \
  cstrnutl.h cstrnchk.h cstrnops.h drive.h inscom.h insfun.h insmngr.h \
  memalloc.h pattern.h reorder.h prntutil.h reteutil.h rulepsr.h \
  exprnpsr.h objrtgen.h objrtfnx.h pprint.h router.h objrtbin.h \
  objrtcmp.h objrtbld.h

objrtcmp.o: objrtcmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  conscomp.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  classcom.h cstrccom.h object.h constrnt.h multifld.h match.h network.h \
  ruledef.h agenda.h crstrtgy.h objrtmch.h objrtfnx.h pattern.h \
  scanner.h reorder.h sysdep.h objrtcmp.h

objrtfnx.o: objrtfnx.c setup.h envrnmnt.h entities.h usrsetup.h \
  classcom.h cstrccom.h moduldef.h userdata.h utility.h evaluatn.h \
  constant.h constrct.h object.h constrnt.h expressn.h exprnops.h \
  multifld.h symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h extnfunc.h symblcmp.h objrtmch.h classfun.h scanner.h \
  bload.h exprnbin.h sysdep.h symblbin.h drive.h engine.h lgcldpnd.h \
  retract.h memalloc.h prntutil.h reteutil.h router.h objrtfnx.h

objrtgen.o: objrtgen.c setup.h envrnmnt.h entities.h usrsetup.h \
  classfun.h object.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h constrnt.h expressn.h exprnops.h multifld.h \
  symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  extnfunc.h symblcmp.h cstrccom.h objrtmch.h scanner.h classcom.h \
  objrtfnx.h objrtgen.h reorder.h pattern.h

objrtmch.o: objrtmch.c setup.h envrnmnt.h entities.h usrsetup.h \
  classfun.h object.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h constrnt.h expressn.h exprnops.h multifld.h \
  symbol.h match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h \
  extnfunc.h symblcmp.h cstrccom.h objrtmch.h scanner.h classcom.h \
  memalloc.h drive.h engine.h lgcldpnd.h retract.h incrrset.h objrtfnx.h \
  prntutil.h reteutil.h ruledlt.h reorder.h pattern.h router.h insmngr.h \
  inscom.h insfun.h

parsefun.o: parsefun.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h cstrcpsr.h strngfun.h exprnpsr.h \
  extnfunc.h symbol.h scanner.h memalloc.h multifld.h pprint.h \
  prcdrpsr.h constrnt.h prntutil.h router.h strngrtr.h parsefun.h

pattern.o: pattern.c setup.h envrnmnt.h entities.h usrsetup.h constant.h \
  constrnt.h evaluatn.h cstrnchk.h cstrnutl.h exprnpsr.h extnfunc.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  symbol.h scanner.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h symblcmp.h cstrccom.h memalloc.h pprint.h prntutil.h \
  reteutil.h router.h rulecmp.h pattern.h reorder.h

pprint.o: pprint.c setup.h envrnmnt.h entities.h usrsetup.h constant.h \
  memalloc.h sysdep.h utility.h evaluatn.h moduldef.h userdata.h \
  pprint.h

prccode.o: prccode.c setup.h envrnmnt.h entities.h usrsetup.h memalloc.h \
  constant.h globlpsr.h expressn.h exprnops.h constrct.h userdata.h \
  moduldef.h utility.h evaluatn.h exprnpsr.h extnfunc.h symbol.h \
  scanner.h multifld.h object.h constrnt.h match.h network.h ruledef.h \
  agenda.h crstrtgy.h conscomp.h symblcmp.h cstrccom.h objrtmch.h \
  pprint.h prcdrpsr.h prntutil.h router.h prccode.h

prcdrfun.o: prcdrfun.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h constrnt.h cstrnchk.h cstrnops.h \
  exprnpsr.h extnfunc.h symbol.h scanner.h memalloc.h multifld.h \
  prcdrpsr.h router.h prcdrfun.h globldef.h conscomp.h symblcmp.h \
  cstrccom.h

prcdrpsr.o: prcdrpsr.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h constrnt.h cstrnchk.h cstrnops.h \
  cstrnutl.h exprnpsr.h extnfunc.h symbol.h scanner.h memalloc.h \
  modulutl.h multifld.h pprint.h prntutil.h router.h prcdrpsr.h \
  globldef.h conscomp.h symblcmp.h cstrccom.h globlpsr.h

prdctfun.o: prdctfun.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h exprnpsr.h extnfunc.h symbol.h \
  scanner.h multifld.h router.h prdctfun.h

prntutil.o: prntutil.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h cstrcpsr.h strngfun.h factmngr.h \
  conscomp.h extnfunc.h symbol.h symblcmp.h tmpltdef.h constrnt.h \
  factbld.h network.h match.h ruledef.h agenda.h crstrtgy.h cstrccom.h \
  facthsh.h inscom.h insfun.h object.h multifld.h objrtmch.h insmngr.h \
  memalloc.h multifun.h router.h scanner.h strngrtr.h sysdep.h \
  prntutil.h

proflfun.o: proflfun.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h classcom.h cstrccom.h object.h \
  constrnt.h multifld.h symbol.h match.h network.h ruledef.h agenda.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h objrtmch.h dffnxfun.h \
  genrccom.h genrcfun.h memalloc.h msgcom.h msgpass.h router.h sysdep.h \
  proflfun.h

reorder.o: reorder.c setup.h envrnmnt.h entities.h usrsetup.h cstrnutl.h \
  constrnt.h evaluatn.h constant.h extnfunc.h expressn.h exprnops.h \
  constrct.h userdata.h moduldef.h utility.h symbol.h memalloc.h \
  pattern.h scanner.h match.h network.h ruledef.h agenda.h crstrtgy.h \
  conscomp.h symblcmp.h cstrccom.h reorder.h prntutil.h router.h \
  rulelhs.h

reteutil.o: reteutil.c setup.h envrnmnt.h entities.h usrsetup.h drive.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h match.h network.h ruledef.h agenda.h symbol.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  engine.h lgcldpnd.h retract.h incrrset.h memalloc.h pattern.h \
  scanner.h reorder.h prntutil.h router.h rulecom.h reteutil.h

retract.o: retract.c setup.h envrnmnt.h entities.h usrsetup.h agenda.h \
  ruledef.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h expressn.h exprnops.h network.h match.h conscomp.h \
  extnfunc.h symbol.h symblcmp.h constrnt.h cstrccom.h crstrtgy.h \
  argacces.h drive.h engine.h lgcldpnd.h retract.h memalloc.h prntutil.h \
  reteutil.h router.h

router.o: router.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h extnfunc.h symbol.h filertr.h memalloc.h \
  prntutil.h scanner.h strngrtr.h sysdep.h router.h

rulebin.o: rulebin.c setup.h envrnmnt.h entities.h usrsetup.h agenda.h \
  ruledef.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h expressn.h exprnops.h network.h match.h conscomp.h \
  extnfunc.h symbol.h symblcmp.h constrnt.h cstrccom.h crstrtgy.h \
  bload.h exprnbin.h sysdep.h symblbin.h bsave.h engine.h lgcldpnd.h \
  retract.h memalloc.h pattern.h scanner.h reorder.h reteutil.h \
  rulebsc.h rulebin.h cstrcbin.h modulbin.h

rulebld.o: rulebld.c setup.h envrnmnt.h entities.h usrsetup.h constant.h \
  constrct.h userdata.h moduldef.h utility.h evaluatn.h drive.h \
  expressn.h exprnops.h match.h network.h ruledef.h agenda.h symbol.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  incrrset.h memalloc.h pattern.h scanner.h reorder.h prntutil.h \
  reteutil.h router.h rulebld.h rulepsr.h watch.h

rulebsc.o: rulebsc.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h drive.h match.h network.h ruledef.h agenda.h \
  symbol.h crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h \
  cstrccom.h engine.h lgcldpnd.h retract.h multifld.h reteutil.h \
  router.h watch.h rulebin.h cstrcbin.h modulbin.h rulecmp.h rulebsc.h

rulecmp.o: rulecmp.c setup.h envrnmnt.h entities.h usrsetup.h factbld.h \
  network.h match.h ruledef.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h expressn.h exprnops.h agenda.h symbol.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  pattern.h scanner.h reorder.h reteutil.h rulecmp.h

rulecom.o: rulecom.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h crstrtgy.h agenda.h ruledef.h network.h match.h \
  conscomp.h extnfunc.h symbol.h symblcmp.h constrnt.h cstrccom.h \
  engine.h lgcldpnd.h retract.h incrrset.h memalloc.h multifld.h \
  pattern.h scanner.h reorder.h prntutil.h reteutil.h router.h ruledlt.h \
  sysdep.h watch.h rulebin.h cstrcbin.h modulbin.h rulecom.h

rulecstr.o: rulecstr.c setup.h envrnmnt.h entities.h usrsetup.h \
  analysis.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h reorder.h pattern.h symbol.h scanner.h \
  match.h network.h ruledef.h agenda.h crstrtgy.h conscomp.h extnfunc.h \
  symblcmp.h constrnt.h cstrccom.h cstrnchk.h cstrnops.h cstrnutl.h \
  prcdrpsr.h prntutil.h router.h rulepsr.h rulecstr.h

ruledef.o: ruledef.c setup.h envrnmnt.h entities.h usrsetup.h agenda.h \
  ruledef.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h expressn.h exprnops.h network.h match.h conscomp.h \
  extnfunc.h symbol.h symblcmp.h constrnt.h cstrccom.h crstrtgy.h \
  drive.h engine.h lgcldpnd.h retract.h memalloc.h pattern.h scanner.h \
  reorder.h reteutil.h rulebsc.h rulecom.h rulepsr.h ruledlt.h bload.h \
  exprnbin.h sysdep.h symblbin.h rulebin.h cstrcbin.h modulbin.h \
  rulecmp.h

ruledlt.o: ruledlt.c setup.h envrnmnt.h entities.h usrsetup.h agenda.h \
  ruledef.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h expressn.h exprnops.h network.h match.h conscomp.h \
  extnfunc.h symbol.h symblcmp.h constrnt.h cstrccom.h crstrtgy.h \
  bload.h exprnbin.h sysdep.h symblbin.h drive.h engine.h lgcldpnd.h \
  retract.h memalloc.h pattern.h scanner.h reorder.h reteutil.h \
  ruledlt.h

rulelhs.o: rulelhs.c setup.h envrnmnt.h entities.h usrsetup.h agenda.h \
  ruledef.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h expressn.h exprnops.h network.h match.h conscomp.h \
  extnfunc.h symbol.h symblcmp.h constrnt.h cstrccom.h crstrtgy.h \
  argacces.h cstrnchk.h exprnpsr.h scanner.h memalloc.h pattern.h \
  reorder.h pprint.h prntutil.h router.h rulelhs.h

rulepsr.o: rulepsr.c setup.h envrnmnt.h entities.h usrsetup.h analysis.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h reorder.h pattern.h symbol.h scanner.h match.h \
  network.h ruledef.h agenda.h crstrtgy.h conscomp.h extnfunc.h \
  symblcmp.h constrnt.h cstrccom.h cstrcpsr.h strngfun.h cstrnchk.h \
  cstrnops.h engine.h lgcldpnd.h retract.h exprnpsr.h incrrset.h \
  memalloc.h modulutl.h prccode.h prcdrpsr.h pprint.h prntutil.h \
  router.h rulebld.h rulebsc.h rulecstr.h ruledlt.h rulelhs.h watch.h \
  tmpltfun.h factmngr.h tmpltdef.h factbld.h facthsh.h bload.h \
  exprnbin.h sysdep.h symblbin.h rulepsr.h

scanner.o: scanner.c setup.h envrnmnt.h entities.h usrsetup.h constant.h \
  memalloc.h pprint.h prntutil.h router.h symbol.h sysdep.h utility.h \
  evaluatn.h moduldef.h userdata.h scanner.h

sortfun.o: sortfun.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h dffnxfun.h extnfunc.h symbol.h memalloc.h \
  multifld.h sysdep.h sortfun.h

strngfun.o: strngfun.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h commline.h cstrcpsr.h strngfun.h \
  engine.h lgcldpnd.h match.h network.h ruledef.h agenda.h symbol.h \
  crstrtgy.h conscomp.h extnfunc.h symblcmp.h constrnt.h cstrccom.h \
  retract.h exprnpsr.h scanner.h memalloc.h miscfun.h multifld.h \
  prcdrpsr.h pprint.h prntutil.h router.h strngrtr.h sysdep.h drive.h

strngrtr.o: strngrtr.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h memalloc.h prntutil.h router.h sysdep.h strngrtr.h \
  utility.h evaluatn.h moduldef.h userdata.h

symblbin.o: symblbin.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h bload.h extnfunc.h symbol.h exprnbin.h \
  sysdep.h symblbin.h bsave.h cstrnbin.h constrnt.h exprnpsr.h scanner.h \
  memalloc.h router.h

symblcmp.o: symblcmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h conscomp.h extnfunc.h symbol.h \
  symblcmp.h cstrncmp.h constrnt.h cstrccom.h exprnpsr.h scanner.h \
  memalloc.h prntutil.h router.h sysdep.h

symbol.o: symbol.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h memalloc.h multifld.h prntutil.h router.h \
  sysdep.h symbol.h

sysdep.o: sysdep.c setup.h envrnmnt.h entities.h usrsetup.h memalloc.h \
  sysdep.h

textpro.o: textpro.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h commline.h extnfunc.h symbol.h memalloc.h \
  prntutil.h router.h sysdep.h textpro.h

tmpltbin.o: tmpltbin.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h bsave.h cstrnbin.h constrnt.h factbin.h factbld.h network.h \
  match.h ruledef.h agenda.h crstrtgy.h conscomp.h symblcmp.h cstrccom.h \
  factmngr.h tmpltdef.h facthsh.h memalloc.h tmpltpsr.h tmpltutl.h \
  tmpltbin.h cstrcbin.h modulbin.h

tmpltbsc.o: tmpltbsc.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h cstrccom.h cstrcpsr.h strngfun.h \
  extnfunc.h symbol.h factrhs.h factmngr.h conscomp.h symblcmp.h \
  tmpltdef.h constrnt.h factbld.h network.h match.h ruledef.h agenda.h \
  crstrtgy.h facthsh.h scanner.h memalloc.h multifld.h router.h \
  tmpltbin.h cstrcbin.h modulbin.h tmpltcmp.h tmpltpsr.h tmpltutl.h \
  tmpltbsc.h

tmpltcmp.o: tmpltcmp.c setup.h envrnmnt.h entities.h usrsetup.h \
  conscomp.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constant.h extnfunc.h expressn.h exprnops.h symbol.h symblcmp.h \
  cstrncmp.h constrnt.h factcmp.h network.h match.h ruledef.h agenda.h \
  crstrtgy.h cstrccom.h pattern.h scanner.h reorder.h tmpltdef.h \
  factbld.h tmpltcmp.h

tmpltdef.o: tmpltdef.c setup.h envrnmnt.h entities.h usrsetup.h \
  cstrccom.h moduldef.h userdata.h utility.h evaluatn.h constant.h \
  constrct.h cstrnchk.h constrnt.h exprnops.h expressn.h memalloc.h \
  modulpsr.h symbol.h scanner.h modulutl.h network.h match.h ruledef.h \
  agenda.h crstrtgy.h conscomp.h extnfunc.h symblcmp.h pattern.h \
  reorder.h router.h tmpltbsc.h tmpltdef.h factbld.h tmpltfun.h \
  factmngr.h facthsh.h tmpltpsr.h tmpltutl.h bload.h exprnbin.h sysdep.h \
  symblbin.h tmpltbin.h cstrcbin.h modulbin.h tmpltcmp.h

tmpltfun.o: tmpltfun.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h commline.h cstrnchk.h constrnt.h \
  default.h exprnpsr.h extnfunc.h symbol.h scanner.h factmngr.h \
  conscomp.h symblcmp.h tmpltdef.h factbld.h network.h match.h ruledef.h \
  agenda.h crstrtgy.h cstrccom.h facthsh.h factrhs.h memalloc.h \
  modulutl.h multifld.h pprint.h prcdrpsr.h prntutil.h reorder.h \
  pattern.h router.h sysdep.h tmpltlhs.h tmpltrhs.h tmpltutl.h \
  tmpltfun.h

tmpltlhs.o: tmpltlhs.c setup.h envrnmnt.h entities.h usrsetup.h \
  constant.h constrct.h userdata.h moduldef.h utility.h evaluatn.h \
  constrnt.h exprnpsr.h extnfunc.h expressn.h exprnops.h symbol.h \
  scanner.h factrhs.h factmngr.h conscomp.h symblcmp.h tmpltdef.h \
  factbld.h network.h match.h ruledef.h agenda.h crstrtgy.h cstrccom.h \
  facthsh.h memalloc.h modulutl.h pattern.h reorder.h pprint.h \
  prntutil.h router.h tmpltutl.h tmpltlhs.h

tmpltpsr.o: tmpltpsr.c setup.h envrnmnt.h entities.h usrsetup.h bload.h \
  utility.h evaluatn.h constant.h moduldef.h userdata.h extnfunc.h \
  expressn.h exprnops.h constrct.h symbol.h exprnbin.h sysdep.h \
  symblbin.h cstrcpsr.h strngfun.h cstrnchk.h constrnt.h cstrnpsr.h \
  cstrnutl.h default.h exprnpsr.h scanner.h factmngr.h conscomp.h \
  symblcmp.h tmpltdef.h factbld.h network.h match.h ruledef.h agenda.h \
  crstrtgy.h cstrccom.h facthsh.h memalloc.h modulutl.h pattern.h \
  reorder.h pprint.h prntutil.h router.h tmpltbsc.h watch.h tmpltpsr.h

tmpltrhs.o: tmpltrhs.c setup.h envrnmnt.h entities.h usrsetup.h default.h \
  constrnt.h evaluatn.h constant.h extnfunc.h expressn.h exprnops.h \
  constrct.h userdata.h moduldef.h utility.h symbol.h factrhs.h \
  factmngr.h conscomp.h symblcmp.h tmpltdef.h factbld.h network.h \
  match.h ruledef.h agenda.h crstrtgy.h cstrccom.h facthsh.h scanner.h \
  memalloc.h modulutl.h pprint.h prntutil.h router.h tmpltfun.h \
  tmpltlhs.h tmpltutl.h tmpltrhs.h

tmpltutl.o: tmpltutl.c setup.h envrnmnt.h entities.h usrsetup.h \
  argacces.h expressn.h exprnops.h constrct.h userdata.h moduldef.h \
  utility.h evaluatn.h constant.h cstrnchk.h constrnt.h extnfunc.h \
  symbol.h memalloc.h modulutl.h scanner.h multifld.h prntutil.h \
  router.h sysdep.h tmpltbsc.h tmpltdef.h conscomp.h symblcmp.h \
  factbld.h network.h match.h ruledef.h agenda.h crstrtgy.h cstrccom.h \
  tmpltfun.h factmngr.h facthsh.h tmpltpsr.h watch.h tmpltutl.h

userdata.o: userdata.c setup.h envrnmnt.h entities.h usrsetup.h \
  userdata.h

userfunctions.o: userfunctions.c clips.h setup.h envrnmnt.h entities.h \
  usrsetup.h argacces.h expressn.h exprnops.h constrct.h userdata.h \
  moduldef.h utility.h evaluatn.h constant.h memalloc.h cstrcpsr.h \
  strngfun.h fileutil.h envrnbld.h extnfunc.h symbol.h commline.h \
  prntutil.h router.h filertr.h strngrtr.h iofun.h sysdep.h bmathfun.h \
  exprnpsr.h scanner.h miscfun.h watch.h modulbsc.h bload.h exprnbin.h \
  symblbin.h bsave.h ruledef.h network.h match.h agenda.h crstrtgy.h \
  conscomp.h symblcmp.h constrnt.h cstrccom.h rulebsc.h engine.h \
  lgcldpnd.h retract.h drive.h incrrset.h rulecom.h dffctdef.h \
  dffctbsc.h tmpltdef.h factbld.h tmpltbsc.h tmpltfun.h factmngr.h \
  facthsh.h factcom.h factfun.h globldef.h globlbsc.h globlcom.h \
  dffnxfun.h genrccom.h genrcfun.h classcom.h object.h multifld.h \
  objrtmch.h classexm.h classfun.h classinf.h classini.h classpsr.h \
  defins.h inscom.h insfun.h insfile.h insmngr.h msgcom.h msgpass.h

utility.o: utility.c setup.h envrnmnt.h entities.h usrsetup.h commline.h \
  evaluatn.h constant.h factmngr.h conscomp.h constrct.h userdata.h \
  moduldef.h utility.h extnfunc.h expressn.h exprnops.h symbol.h \
  symblcmp.h tmpltdef.h constrnt.h factbld.h network.h match.h ruledef.h \
  agenda.h crstrtgy.h cstrccom.h facthsh.h memalloc.h multifld.h \
  prntutil.h router.h sysdep.h

watch.o: watch.c setup.h envrnmnt.h entities.h usrsetup.h argacces.h \
  expressn.h exprnops.h constrct.h userdata.h moduldef.h utility.h \
  evaluatn.h constant.h extnfunc.h symbol.h memalloc.h router.h watch.h
