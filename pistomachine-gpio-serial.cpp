#include <cstdio>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <boost/program_options.hpp>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>

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

	if(uinputfile == ""){
		input_event ev;
		while(int r = read(STDIN_FILENO, (char*)&ev, sizeof(ev))){
			if(r != sizeof(ev)) throw "reading device";
			if(ev.type != EV_KEY || !ev.code || ev.code >= 128) continue;
			char over_serial = ev.code | (ev.value * 0x80);
			if(TEMP_FAILURE_RETRY(write(STDOUT_FILENO, &over_serial, 1)) != 1) throw "piping to serial";
		}
	}
	else try {
		int vkeyb = open(uinputfile.c_str(), O_WRONLY);
		{
			if(vkeyb == -1) throw "open uinput file";
			if(ioctl(vkeyb, UI_SET_EVBIT, EV_KEY) || ioctl(vkeyb, UI_SET_EVBIT, EV_SYN)) throw "UI_SET_EVBIT";
			for(int i = 1; i < 128; i++) if(ioctl(vkeyb, UI_SET_KEYBIT, mappings[i] ?: i)) throw "UI_SET_KEYBIT";
			uinput_user_dev uidev;
			memset(&uidev, 0, sizeof(uidev));
			strncpy(uidev.name, devicename.c_str(), UINPUT_MAX_NAME_SIZE);
			uidev.id.bustype = BUS_VIRTUAL;
			if(write(vkeyb, &uidev, sizeof(uidev)) != sizeof(uidev)) throw "writing device specification";
			if(ioctl(vkeyb, UI_DEV_CREATE)) throw "UI_DEV_CREATE";
		}

		input_event ev[2];
		memset(&ev, 0, sizeof(ev));
		ev[0].type = EV_KEY;
		ev[1].type = EV_SYN;
		ev[1].code = SYN_REPORT;
		unsigned char in;
		while(int r = TEMP_FAILURE_RETRY(read(STDIN_FILENO, &in, 1))){
			if(r != sizeof(ev)) throw "reading serial";
			ev[0].value = !!(in & 0x80);
			in &= ~0x80;
			if(!in) continue;
			ev[0].code = mappings[in] ?: in;
			if(write(vkeyb, &ev, sizeof(ev)) != sizeof(ev)) throw "event generation";
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
