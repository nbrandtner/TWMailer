LDLIBS += -lpthread -lldap

rebuild: clean all

all: twmailer-server twmailer-client

clean:
	rm -f twmailer-client twmailer-server

twmailer-server: TWMailerServer
	g++ -std=c++14 -Wall -Werror -o twmailer-server TWMailerServer.cpp $(LDLIBS)

twmailer-client: TWMailerClient
	g++ -std=c++17 -Wall -Werror -o twmailer-client TWMailerClient.cpp $(LDLIBS)
