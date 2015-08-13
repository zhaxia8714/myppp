OBJECTS := myppp
all:
	$(CC) -g -o $(OBJECTS) -I./ main.c crc.c hdlc.c ppp_engine.c ppp_interface.c ppp_lcp.c ppp_ip6cp.c virtual_timer.c  $(LDFLAGS)

.PHONY: clean
clean:
	@rm -f  $(OBJECTS) *~ $(OBJECTS)

