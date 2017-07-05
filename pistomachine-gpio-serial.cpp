#include <cstdio>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <boost/program_options.hpp>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <linux/serial.h>

using namespace std;
using namespace boost::program_options;

namespace {
options_description options("Options for pistomachine-gpio-serial");
unsigned char mappings[128];
}

int main(int argc, char** argv) try {
	string uinputfile, devicename;
	{
		vector<string> mappings_arg;
		options.add_options()
					("help,h", "print help")
					("uinput,u", value(&uinputfile), "uinput device (activates decode mode)")
					("map,m", value(&mappings_arg), "mapping (from:to)")
					("name,n", value(&devicename)->default_value("pistomachine-gpio"), "virtual device name")
					;
		variables_map vm;
		store(parse_command_line(argc, argv, options), vm);
		notify(vm);
		if(vm.count("help")){
			cerr<<options<<endl;
			return 0;
		}
		if(devicename == "") throw invalid_argument("device name must not be empty");
		regex map_regex("(\\d{1,3}):(\\d{1,3})");
		for(auto& ms: mappings_arg){
			smatch match;
			if(!regex_match(ms, match, map_regex)) throw invalid_argument("Invalid format for mapping " + ms);
			int from = stoi(match[0]), to = stoi(match[1]);
			if(!to || !from || from >= 128) throw invalid_argument("Invalid mapping");
			mappings[from] = to;
		}
	}

	auto set_serial_mode = [](int fd){
		termios term;
		serial_struct serial;
		try {
			#define checkerr(f, ...) if(f(__VA_ARGS__)) throw #f
			checkerr(tcgetattr, fd, &term);
			checkerr(cfsetspeed, &term, B115200);
			term.c_cflag |= CLOCAL;
			memset(term.c_cc, _POSIX_VDISABLE, sizeof(term.c_cc));
			term.c_cc[VMIN] = 1;
			term.c_cc[VTIME] = 0;
			cfmakeraw(&term);
			checkerr(tcsetattr, fd, TCSANOW, &term);
			checkerr(ioctl, fd, TIOCGSERIAL, &serial);
			serial.flags |= ASYNC_LOW_LATENCY;
			checkerr(ioctl, fd, TIOCSSERIAL, &serial);
			#undef checkerr
		} catch(const char* at){
			if(errno == ENOTTY) return;
			throw;
		}
	};

	try {
		if(uinputfile == ""){
			set_serial_mode(STDOUT_FILENO);
			input_event ev;
			while(int r = read(STDIN_FILENO, (char*)&ev, sizeof(ev))){
				if(r != sizeof(ev)) throw "reading device";
				if(ev.type != EV_KEY || !ev.code || ev.code >= 128) continue;
				unsigned char over_serial = ev.code | (!!ev.value * 0x80);
				if(TEMP_FAILURE_RETRY(write(STDOUT_FILENO, &over_serial, 1)) != 1) throw "piping to serial";
			}
		} else {
			set_serial_mode(STDIN_FILENO);
			int vkeyb = open(uinputfile.c_str(), O_WRONLY);
			if(vkeyb == -1) throw "open uinput file";
			if(ioctl(vkeyb, UI_SET_EVBIT, EV_KEY) || ioctl(vkeyb, UI_SET_EVBIT, EV_SYN)) throw "UI_SET_EVBIT";
			for(int i = 1; i < 128; i++) if(ioctl(vkeyb, UI_SET_KEYBIT, mappings[i] ?: i)) throw "UI_SET_KEYBIT";
			uinput_user_dev uidev;
			memset(&uidev, 0, sizeof(uidev));
			strncpy(uidev.name, devicename.c_str(), UINPUT_MAX_NAME_SIZE);
			uidev.id.bustype = BUS_VIRTUAL;
			if(write(vkeyb, &uidev, sizeof(uidev)) != sizeof(uidev)) throw "writing device specification";
			if(ioctl(vkeyb, UI_DEV_CREATE)) throw "UI_DEV_CREATE";

			input_event ev[2];
			memset(&ev, 0, sizeof(ev));
			ev[0].type = EV_KEY;
			ev[1].type = EV_SYN;
			ev[1].code = SYN_REPORT;
			unsigned char in;
			while(int r = TEMP_FAILURE_RETRY(read(STDIN_FILENO, &in, 1))){
				if(r != 1) throw "reading serial";
				ev[0].value = !!(in & 0x80);
				in &= ~0x80;
				if(!in) continue;
				ev[0].code = mappings[in] ?: in;
				if(write(vkeyb, &ev, sizeof(ev)) != sizeof(ev)) throw "event generation";
			}
		}
	} catch(const char* at){
		throw runtime_error(string(at) + " (" + strerror(errno) + ")");
	}

} catch(const invalid_argument& e){
	cerr<<"Invalid argument found in command line: "<<e.what()<<endl<<options<<endl;
	return 1;
} catch(const boost::program_options::error& e){
	cerr<<"Failed to parse command line: "<<e.what()<<endl<<options<<endl;
	return 1;
} catch(const runtime_error& e){
	cerr<<"Error: "<<e.what()<<endl;
	return 1;
}
