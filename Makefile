CXXFLAGS:= -std=c++1y
LDLIBS:= -lboost_program_options

release: CXXFLAGS+= -O3
release: LDFLAGS+= -s
release: pistomachine-gpio-serial
debug: CXXFLAGS+= -ggdb3
debug: pistomachine-gpio-serial
sanitize: CXXFLAGS+= -fsanitize=address,undefined
sanitize: pistomachine-gpio-serial

pistomachine-keys.dtbo: pistomachine-keys.dts
	dtc -I dts -O dtb -@ -o pistomachine-keys.dtbo pistomachine-keys.dts

.DEFAULT_GOAL := release

clean:
	rm -f pistomachine-gpio-serial pistomachine-keys.dtbo

.PHONY: clean release debug sanitize
