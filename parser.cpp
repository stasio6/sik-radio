#include <algorithm>
#include <iostream>
#include <string.h>

#include "parser.h"

using namespace std;

void check(bool &letter, void (*print_usage)()) {
    if (letter)
        print_usage();
    letter = true;
}

void check_if_number(char *c, string variable) {
    string str(c);
    if (str.empty() || str.size() > 9 || !all_of(str.begin(), str.end(), ::isdigit)) {
        cerr << variable << " should be a non-negative integer\n";
        exit(1);
    }
}

proxy_params parse_proxy_params(int argc, char *argv[], void (*print_usage)()) {
    if (argc % 2 != 1) {
        print_usage();
    }
    proxy_params params;
    params.timeout = 5;
    params.metadata = 0;
    params.multicast_address = "";
    params.agent_timeout = 5;
    params.agent_active = false;
    bool h = false, r = false, p = false, m = false, t = false, P = false, B = false, T = false;
    for (int i = 1; i < argc; i += 2) {
        if (strlen(argv[i]) != 2 || argv[i][0] != '-')
            print_usage();
        switch (argv[i][1]) {
            case 'h':
                check(h, print_usage);
                params.host = argv[i+1];
                break;
            case 'r':
                check(r, print_usage);
                params.resource = argv[i+1];
                break;
            case 'p':
                check(p, print_usage);
                check_if_number(argv[i+1], "port");
                params.port = atoi(argv[i+1]);
                break;
            case 'm':
                check(m, print_usage);
                if (!strcmp(argv[i+1], "no"))
                    params.metadata = 0;
                else if (!strcmp(argv[i+1], "yes"))
                    params.metadata = 1;
                else
                    print_usage();
                break;
            case 't':
                check(t, print_usage);
                check_if_number(argv[i+1], "timeout");
                params.timeout = atoi(argv[i+1]);
                if (params.timeout == 0)
                    print_usage();
                break;
            case 'P':
                check(P, print_usage);
                check_if_number(argv[i+1], "agent_port");
                params.agent_port = atoi(argv[i+1]);
                params.agent_active = true;
                break;
            case 'B':
                check(B, print_usage);
                params.multicast_address = argv[i+1];
                break;
            case 'T':
                check(T, print_usage);
                check_if_number(argv[i+1], "agent_timeout");
                params.agent_timeout = atoi(argv[i+1]);
                if (params.agent_timeout == 0)
                    print_usage();
                break;
            default:
                print_usage();
        }
    }
    if ((B || T) && !P)
        print_usage();
    if (!h || !r || !p)
        print_usage();
    return params;
}

client_params parse_client_params(int argc, char *argv[], void (*print_usage)()) {
    if (argc % 2 != 1) {
        print_usage();
    }
    client_params params;
    params.timeout = 5;
    bool H = false, P = false, p = false, T = false;
    for (int i = 1; i < argc; i += 2) {
        if (strlen(argv[i]) != 2 || argv[i][0] != '-')
            print_usage();
        switch (argv[i][1]) {
            case 'H':
                check(H, print_usage);
                params.host = argv[i+1];
                break;
            case 'P':
                check(P, print_usage);
                check_if_number(argv[i+1], "port");
                params.port = atoi(argv[i+1]);
                break;
            case 'p':
                check(p, print_usage);
                check_if_number(argv[i+1], "control_port");
                params.control_port = atoi(argv[i+1]);
                break;
            case 'T':
                check(T, print_usage);
                check_if_number(argv[i+1], "timeout");
                params.timeout = atoi(argv[i+1]);
                if (params.timeout == 0)
                    print_usage();
                break;
            default:
                print_usage();
        }
    }
    if (!H || !P || !p)
        print_usage();
    return params;
}