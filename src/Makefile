CXXFLAGS = -Wall -g

REP_LAYER = Instance.o
ENGINE_LAYER = Engine.o
OBJECTS = $(REP_LAYER) $(ENGINE_LAYER) ActivityReactor.o ActivityImpl.o
SIDE_CODE = snippets.o

ALL_OBJECTS = $(SIDE_CODE) $(OBJECTS) example.o example2.o client.o verification.o experiment.o test1.o activity.o
EXECUTABLES = test1 example verification experiment example2 snippets client

REP_LIBS = fwk/Ptr.h fwk/PtrInterface.h
ENGINE_LIBS = fwk/BaseCollection.o fwk/BaseNotifiee.o fwk/Exception.o Nominal.h


default: test1 example example2 client verification experiment

test1: test1.o $(OBJECTS) $(ENGINE_LIBS) 
	$(CXX) $(CXXFLAGS) -o $@ $^

client: client.o $(OBJECTS) $(ENGINE_LIBS) 
	$(CXX) $(CXXFLAGS) -o $@ $^

example: example.o $(OBJECTS) $(ENGINE_LIBS) 
	$(CXX) $(CXXFLAGS) -o $@ $^

example2: example2.o $(OBJECTS) $(ENGINE_LIBS) 
	$(CXX) $(CXXFLAGS) -o $@ $^

verification: verification.o $(OBJECTS) $(ENGINE_LIBS) 
	$(CXX) $(CXXFLAGS) -o $@ $^

experiment: experiment.o $(OBJECTS) $(ENGINE_LIBS) 
	$(CXX) $(CXXFLAGS) -o $@ $^

snippets: snippets.cpp $(ENGINE_LAYER) $(ENGINE_LIBS) 
	$(CXX) $(CXXFLAGS) -o $@ $^	

clean:
	rm -f $(EXECUTABLES) $(ALL_OBJECTS) *~


SUBDIRS = fwk/
.PHONY: subdirs $(SUBDIRS)
all: clean $(SUBDIRS) default
$(SUBDIRS):
	$(MAKE) clean -C $@ && $(MAKE) -C $@
 

Engine.o: Engine.h Engine.cpp ActivityReactor.o
Instance.o: Instance.cpp Instance.h $(REP_LIBS) Engine.o ActivityReactor.o

test1.o: test1.cpp $(OBJECTS)
example.o: example.cpp $(REP_LAYER)
example2.o: example2.cpp $(REP_LAYER)
client.o: client.cpp $(REP_LAYER)
verification.o: verification.cpp $(REP_LAYER)
experiment.o: experiment.cpp $(REP_LAYER)
ActivityImpl.o: ActivityImpl.h ActivityImpl.cpp
ActivityReactor.o: ActivityReactor.h ActivityReactor.cpp ActivityImpl.o