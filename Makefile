# Project: AudacityRover

OBJ_COMMUNICATION = Communication/CommunicationJumpropes.o
OBJ_COMMAND = Command/CommandQueueDefault.o
OBJ_PILOT = Pilot/RemotePilotGoPiGo.o
OBJ_CAMERA = Camera/CameraRaspi.o
OBJ_MAIN = main.o

OBJ = $(OBJ_COMMUNICATION) $(OBJ_COMMAND) $(OBJ_PILOT) $(OBJ_CAMERA) $(OBJ_MAIN)
LINKOBJ = $(OBJ)
LIBS = -L"./lib/GoPiGo" -L"./lib/Crosscables/libGroundfloor" -L"./lib/Crosscables/libJumpropes" -L"./lib/OpenALRF" -lGoPiGo -lOpenALRF -lJumpropes -lGroundfloor
BIN  = AudacityRover
CXXINCS = -I"./lib" -I"./lib/Crosscables/include"
CXXFLAGS = $(CXXINCS) -fexceptions -std=c++11 -O2
RM = rm -f

.PHONY: all all-before all-after clean clean-custom

all: all-before $(BIN) all-after

clean:
	${RM} $(LINKOBJ) $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(LINKOBJ) -o "$(BIN)" $(LIBS)

Command/CommandQueueDefault.o: Command/CommandQueueDefault.cpp
	$(CXX) -c Command/CommandQueueDefault.cpp -o Command/CommandQueueDefault.o $(CXXFLAGS)

Pilot/RemotePilotGoPiGo.o: Pilot/RemotePilotGoPiGo.cpp
	$(CXX) -c Pilot/RemotePilotGoPiGo.cpp -o Pilot/RemotePilotGoPiGo.o $(CXXFLAGS)

Camera/CameraRaspi.o: Camera/CameraRaspi.cpp
	$(CXX) -c Camera/CameraRaspi.cpp -o Camera/CameraRaspi.o $(CXXFLAGS)

main.o: main.cpp
	$(CXX) -c main.cpp -o main.o $(CXXFLAGS)