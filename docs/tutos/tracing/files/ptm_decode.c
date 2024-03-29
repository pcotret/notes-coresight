/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <getopt.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define false (0)
#define true (1)

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

//#define DISPLAY_DEBUG 
unsigned char *buffer;

#define LOGV(f, args...) fprintf(stdout, f, ## args)
#define LOGD(f, args...) fprintf(stderr, f, ## args)
#define LOGE(f, args...) fprintf(stderr, "error: " f, ## args)

typedef int bool;


struct state {
	uint8_t data;
	uint32_t iaddr;
	uint32_t iaddr_valid;
	uint64_t timestamp;
	int wait_count;
	unsigned int long_wait;
	uint32_t pc; // best guess
	int branch_count;
	int mode; // 2 ARM, 1 Thumb, 0 Jazelle
	int sync;
	uint8_t formatter_packet[16];
	int formatter_index;
	int sourceid;

	int contextid_bytes;
	int sourceid_match;
	bool cycle_accurate;
	bool alt_branch;
	bool program_flow_only;
	bool print_input;
	bool formatter;
	int previous;
	int new;
};

struct packet_type {
	uint8_t match_mask;
	uint8_t match_val;
	uint8_t nonzero_mask;
	int (*decode)(struct state *state);
	const char *name;
};


// afficher l'état des différents éléments de structure state 
void print_state(struct state *state, char str[100])
{
	// nombre de fois que la fonction a été 
	static int count; 
	printf("===================   %s   =======================\n ", str);
	printf("Nombre d'appels = %d \n", count);
	printf("sync = %d\n ", state->sync);
	printf("data = %01x\n ", state->data);
	printf("iaddr = %08x\n ", state->iaddr);
	printf("iaddr_valid = %d\n ", state->iaddr_valid);
	printf("pc = %08x\n ", state->pc);
	printf("branch_count = %d\n ", state->branch_count);
	if (count==0)
	{
		printf("cycle_accurate = %d\n ", state->cycle_accurate);	
		printf("program_flow_only = %d\n ", state->program_flow_only);
		printf("formatter = %d\n ", state->formatter);
	}
	count++;
}

static int get_byte(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "get_byte");
	#endif
	int i;
	int ch;
	static int count = 0;

	ch = buffer[count];
	count++;

	if (ch < 0) {
		if (state->wait_count)
			printf(" Waited %d", state->wait_count);
		exit(0);
	}
	state->data = ch;
	if (!ch && state->sync <= 0) {
		if (state->sync == -4)
			state->sync = 1;
		else
			state->sync--;
	}
	if (state->print_input) {
		printf("%02x, (", ch);
		printf("%02x, ", state->data);
		for (i = 0; i < 8; i++)
			printf("%d", (ch >> (7 - i)) & 1);
		printf(")\n");
	}
	return ch;
}

static char in_sync_char(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "in_sync_char");
	#endif
	return state->sync > 0 ? ' ' : '?';
}

static uint32_t next_pc(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "next_pc");	
	#endif
	uint32_t ret = state->pc;
	if (state->program_flow_only)
		state->branch_count++;
	else
		state->pc += 1 << state->mode;	
	return ret;
}

static void print_instruction(struct state *state, bool execute)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "print_instruction");
	#endif
	int waited = state->wait_count;
	uint32_t pc = next_pc(state);
	if (waited) {
		//printf("  Wait %d\n", state->wait_count);
		state->wait_count = 0;
	}
	if (waited > state->long_wait)
		printf("  ==== Long wait ====\n");
	printf(ANSI_COLOR_BLUE "pc = %08x,(%c)" ANSI_COLOR_RESET, pc, execute ? 'E' : 'N');
	//if (state->program_flow_only)
		//printf(" +%d branch points", state->branch_count);
	if (state->iaddr_valid != 0xffffffff)
		printf(" valid %08x", state->iaddr_valid);
	if (waited)
		printf(" Waited %d", waited);
	printf("\n");
}

static int get_branch_addr(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "get_branch_addr");
	#endif
	int addr = state->iaddr >> state->mode;
	int count = 1;
	uint32_t valid_mask = 0x3f;
	uint8_t mask;
	int ret;

	addr = (addr & ~0x3f) | ((state->data >> 1) & 0x3f);
	if (state->print_input)
		printf("  v %x a %x\n", valid_mask << state->mode, addr << state->mode);
	while (state->data & 0x80 && count < 5) {
		get_byte(state);
		if (state->alt_branch && !(state->data & 0x80))
			mask = 0x3f;
		else
			mask = 0x7f;
		addr = (addr & ~(mask << (7 * count - 1))) | ((state->data & mask) << (7 * count - 1));
		valid_mask ^= mask << (7 * count - 1);
		count++;
		if (state->print_input)
			printf("  v %x a %x\n", valid_mask << state->mode, addr << state->mode);
	}
	ret = (count == 5 || (state->alt_branch && count > 1)) && state->data & 0x40;
	if (!ret && state->program_flow_only) {
		// PFT Branch without exception implies an E atom
		print_instruction(state, true);
	}
	if (count == 5) {
		if (!(state->data & 0xb0))
			state->mode = 2;
		else if (!(state->data  & 0xa0))
			state->mode = 1;
		else
			state->mode = 0;
	}
	addr <<= state->mode;
	valid_mask <<= state->mode | ((1 << state->mode) - 1);
	if (state->iaddr_valid < valid_mask)
		state->iaddr_valid = valid_mask;
	state->iaddr = addr;
	state->pc = addr;
	state->branch_count = 0;
	
	return ret;
}

static int branch(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "branch");
	#endif
	const char *mode_name[] = { "Jazelle", "Thumb", "ARM" };
	uint32_t exception_data;
	int ret;

	ret = get_branch_addr(state);
	if (ret) {
		exception_data = get_byte(state);
		if (exception_data & 0x80)
			exception_data |= get_byte(state);
	}

	printf(ANSI_COLOR_RED "Address %08x, Branch Address packet" ANSI_COLOR_RESET, state->iaddr);
	if (ret)
		printf(" Exception data %04x", exception_data);
	if (state->iaddr_valid != 0xffffffff)
		printf(" (valid %08x)", state->iaddr_valid);
	printf(" %s\n",	mode_name[state->mode]);
	return 0;
}

static int timestamp(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "timestamp");
	#endif
	uint64_t timestamp = state->timestamp;
	uint8_t mask = 0x7f;
	int r = !!(state->data & 4);
	int i;

	for (i = 0; i < 9; i++) {
		get_byte(state);
		if (i == 8)
			mask = 0xff;
		timestamp = (timestamp & ~((uint64_t)mask << (7 * i))) | ((uint64_t)(state->data & mask) << (7 * i));
		if (!(state->data & 0x80))
			break;
	}
	// TODO: Convert from gray-code to binary if ETMCCER[28] is not set
	printf("%c   Timestamp %"PRIu64" (%+"PRIi64"), R %d\n",
		in_sync_char(state), timestamp, timestamp - state->timestamp, r);
	state->timestamp = timestamp;	
	return 0;
}

static int async(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "A-SYNC");
	#endif
	return 0;
}

static int ignore(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "ignore");
	#endif
	return 0;
}

static int ccount(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "ccount");
	#endif
	uint32_t count = 0;
	int i;
	for (i = 0; i < 5; i++) {
		get_byte(state);
		count |= (state->data & 0x7f) << (7 * i);
		if (!(state->data & 0x80))
			break;
	}
	//if (count > state->long_wait)
		//printf("    ==== Long wait ====\n");
	printf("Cycle count %d\n", count);
	return 0;
}

static uint32_t get_contextid(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "get_contextid");
	#endif
	int i;
	uint32_t contextid = 0;
	for (i = 0; i < state->contextid_bytes; i++) {
		get_byte(state);
		contextid |= state->data << (8 * i);
	}
	return contextid;
}

static uint32_t get_addr(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "get_addr");
	#endif
	int i;
	uint32_t addr = 0;
	for (i = 0; i < 4; i++) {
		get_byte(state);
		#ifdef DISPLAY_DEBUG
		printf("get_addr data = %01x \n", state->data);
		#endif
		addr |= state->data << (8 * i);
	}
	return addr;
}

static void update_addr(struct state *state, uint8_t ib, uint32_t addr)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "update_addr");
	#endif
	if (ib & 0x10) {
		state->mode = 0;
	} else if (addr & 1) {
		state->mode = 1;
		addr &= ~1;
	} else {
		state->mode = 2;
	}
	#ifdef DISPLAY_DEBUG
	printf("update_addr before data = %01x \n", state->data);
	#endif
	state->iaddr = addr;
	state->iaddr_valid = 0xffffffff;
	state->pc = addr;
	state->branch_count = 0;
	#ifdef DISPLAY_DEBUG
	printf("update_addr data = %01x \n", state->data);
	#endif
}

static int isync(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "I-SYNC");
	#endif
	uint32_t contextid;
	uint32_t addr;
	uint8_t ib;

	printf(ANSI_COLOR_GREEN "isync data = %01x \n" ANSI_COLOR_RESET, state->data);
	contextid = get_contextid(state);
	ib = get_byte(state);
	addr = get_addr(state);
	update_addr(state, ib, addr);
	printf(ANSI_COLOR_GREEN "Addr %08x\n, I-sync Context %08x, IB %02x" ANSI_COLOR_RESET, state->iaddr, contextid, ib);
	return 0;
}

static int isynccc(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "isynccc");
	printf("isynccc data = %01x \n", state->data);
	#endif
	
	ccount(state);
	return isync(state);
}

static int contextid(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "contextid");
	#endif
	uint32_t contextid;

	contextid = get_contextid(state);
	#ifdef DISPLAY_DEBUG
	printf("contextid data = %01x \n", state->data);
	#endif
	printf("%c ContextID %08x\n", in_sync_char(state), contextid);
	return 0;
}

static int vmid(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "vmid");
	#endif
	uint8_t vmid;

	vmid = get_byte(state);
	printf("%c VMID %08x\n", in_sync_char(state), vmid);
	return 0;
}

static int ndata(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "ndata");
	#endif
	uint8_t h = state->data;
	int size = (h >> 2) & 3;
	int i;
	static uint32_t daddr = 0;
	uint32_t data = 0;
	if (h & 0x20) {
		for (i = 0; i < 5; i++) {
			get_byte(state);
			daddr = (daddr & ~(0x7f << (7 * i))) | (state->data & 0x7f) << (7 * i);
			if (!(state->data & 0x80))
				break;
		}
	}
	for (i = 0; i < size; i++) {
		get_byte(state);
		data |= state->data << (i * 8);
	}
	if (size) {
		if (h & 0x20)
			printf("%c   Normal data %08x addr %08x\n", in_sync_char(state), data, daddr);
		else
			printf("%c   Normal data %08x\n", in_sync_char(state), data);
	} else {
		if (h & 0x20)
			printf("%c   Normal data addr %08x\n", in_sync_char(state), daddr);
		else
			printf("%c   Normal data\n", in_sync_char(state));
	}
	return 0;
}

static int pheader_cycle_accurate(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "pheader_cycle_accurate");
	#endif
	int i;
	//printf("  ");
	if (state->data == 0x80) {
		//printf("W");
		state->wait_count++;
	} else if ((state->data & 0xa3) == 0x80) {
		i = (state->data >> 2) & 0x7;
		while (i--) {
			//printf("WE(%08x)", next_pc(state));
			state->wait_count++;
			print_instruction(state, true);
		}
		i = (state->data >> 6) & 0x1;
		while (i--) {
			//printf("WN(%08x)", next_pc(state));
			state->wait_count++;
			print_instruction(state, false);
		}
	} else if ((state->data & 0xf3) == 0x82) {
		//printf("W%c(%08x)%c(%08x)",
		//	(state->data & 0x08) ? 'N' : 'E', next_pc(state),
		//	(state->data & 0x04) ? 'N' : 'E', next_pc(state));
		state->wait_count++;
		print_instruction(state, !(state->data & 0x08));
		print_instruction(state, !(state->data & 0x04));
	} else if ((state->data & 0xa3) == 0xa0) {
		i = ((state->data >> 2) & 0x7) + 1;
		//while (i--)
		//	printf("W");
		state->wait_count += i;
		i = (state->data >> 6) & 0x1;
		while (i--) {
			//printf("E(%08x)", next_pc(state));
			print_instruction(state, true);
		}
	} else if ((state->data & 0xfb) == 0x92) {
		//printf("%c(%08x)", (state->data & 0x04) ? 'N' : 'E', next_pc(state));
		print_instruction(state, !(state->data & 0x04));
	} else {
		state->sync = 0;
		printf("  ?\n");
	}
	//printf("\n");
	return 0;
}

static int pheader_non_cycle_accurate(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "pheader_non_cycle_accurate");
	#endif
	int i;
	if ((state->data & 0x83) == 0x80) {
		i = (state->data >> 2) & 0x0f;
		while (i--) {
			//printf("E(%08x)", next_pc(state));
			print_instruction(state, true);
		}
		if (state->data & 0x40) {
			//printf("N(%08x)", next_pc(state));
			print_instruction(state, false);
		}
	} else if ((state->data & 0xf3) == 0x82) {
		//printf("%c(%08x)%c(%08x)",
		//	(state->data & 0x08) ? 'N' : 'E', next_pc(state),
		//	(state->data & 0x04) ? 'N' : 'E', next_pc(state));
		print_instruction(state, !(state->data & 0x08));
		print_instruction(state, !(state->data & 0x04));
	} else {
		state->sync = 0;
		printf("  ?\n");
	}
	return 0;
}

static int pheader(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "pheader");
	#endif
	if (state->cycle_accurate)
		return pheader_cycle_accurate(state);
	else
		return pheader_non_cycle_accurate(state);
}

static int cycle_count_pft(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "cycle_count_pft");
	#endif
	int i;
	uint8_t h = state->data;
	uint32_t count = (h >> 2) & 0xf;
	int shift = 4;
	if (h & 0x40) {
		for (i = 0; i < 4; i++) {
			get_byte(state);
			count |= (state->data & 0x7f) << shift;
			shift += 7;
			if (!(state->data & 0x80))
				break;
		}
	}
	/*if (count > state->long_wait)
		printf("    ==== Long wait ====\n");*/
	printf("Cycle count PFT %d\n", count);
	return 0;
}

static int isync_pft(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "isync_pft");
	#endif
	uint32_t contextid;
	uint32_t addr;
	uint8_t ib;

	addr = get_addr(state);
	ib = get_byte(state);
	update_addr(state, ib, addr);

	if (state->cycle_accurate && (ib & 0x60)) {
		get_byte(state);
		cycle_count_pft(state);
	}
	
	contextid = get_contextid(state);

	printf(ANSI_COLOR_GREEN "Address %08x, (I-sync Context %08x, IB %02x)\n" ANSI_COLOR_RESET, state->iaddr, contextid, ib);
	return 0;
}

static int waypoint(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "waypoint");
	#endif
	const char *mode_name[] = { "Jazelle", "Thumb", "ARM" };
	uint8_t ib = 0;
	int ret;

	get_byte(state);
	ret = get_branch_addr(state);
	if (ret)
		ib = get_byte(state);

	printf("Waypoint %08x", state->iaddr);
	if (ret)
		printf(" ib %02x", ib);
	if (state->iaddr_valid != 0xffffffff)
		printf(" (valid %08x)", state->iaddr_valid);
	printf(" %s\n", mode_name[state->mode]);
	return 0;
}

static int branch_pft(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "branch_pft");
	#endif
	int ret;
	ret = branch(state);
	if (!ret && state->cycle_accurate) {
		get_byte(state);
		ret = cycle_count_pft(state);
	}
	return ret;
}

static int timestamp_pft(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "timestamp_pft");
	#endif
	int ret;
	ret = timestamp(state);
	if (!ret && state->cycle_accurate) {
		get_byte(state);
		ret = cycle_count_pft(state);
	}
	return ret;
}

static int atom_cycle_accurate(struct state *state)
{
	uint8_t h = state->data;
	#ifdef DISPLAY_DEBUG
	print_state(state, "atom_cycle_accurate");
	#endif
	cycle_count_pft(state);
	print_instruction(state, !(h & 0x02));
	return 0;
}

static int atom_non_cycle_accurate(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "atom_non_cycle_accurate");
	#endif
	int i;
	uint8_t d = state->data << 1;
	for (i = 5; i > 0; i--, d <<= 1)
		if (d & 0x80)
			break;
	while (i--) {
		d <<= 1;
		print_instruction(state, !(d & 0x80));
	}
	return 0;
}

static int atom(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "Atom");
	#endif
	if (state->cycle_accurate)
		return atom_cycle_accurate(state);
	else
		return atom_non_cycle_accurate(state);
}

static int trigger(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "Trigger");
	#endif
	printf("%c   Trigger\n", in_sync_char(state));
	return 0;
}

static int except_ret(struct state *state)
{
	#ifdef DISPLAY_DEBUG
	print_state(state, "except_ret");
	#endif
	printf("%c   Exception return\n", in_sync_char(state));
	return 0;
}

struct packet_type packet_types_etm_3_3[] = {
	{ 0x01, 0x01, 0x00, branch,	"Branch address"				},
	{ 0xff, 0x00, 0x00, async,	"A-sync"						},
	{ 0xff, 0x04, 0x00, ccount,	"Cycle count"					},
	{ 0xff, 0x08, 0x00, isync,	"I-sync"						},
	{ 0xff, 0x0c, 0x00, NULL,	"Trigger"						},
	{ 0x93, 0x00, 0x60, NULL,	"Out-of-order data"				},
	{ 0xff, 0x50, 0x00, NULL,	"Store failed"					},
	{ 0xff, 0x70, 0x00, isynccc,	"I-sync with cycle count"	},
	{ 0xd3, 0x50, 0x0c, NULL,	"Out-of-order placeholder"		},
	{ 0xd3, 0x12, 0x00, NULL,	"Reserved"			},
	{ 0xf3, 0x10, 0x00, NULL,	"Reserved"			},
	{ 0xfb, 0x30, 0x00, NULL,	"Reserved"			},
	{ 0xff, 0x3c, 0x00, vmid,	"VMID"				},
	{ 0xff, 0x38, 0x00, NULL,	"Reserved"			},
	{ 0xd3, 0x02, 0x00, ndata,	"Normal data"		},
	{ 0xf3, 0x52, 0x00, NULL,	"Reserved"			},
	{ 0xfb, 0x42, 0x00, timestamp,	"Timestamp"		},
	{ 0xfb, 0x4a, 0x00, NULL,	"Reserved"			},
	{ 0xff, 0x62, 0x00, NULL,	"Data suppressed"	},
	{ 0xff, 0x66, 0x00, ignore,	"Ignore"			},
	{ 0xef, 0x6a, 0x00, NULL,	"Value not traced"	},
	{ 0xff, 0x6e, 0x00, contextid,	"Context ID"	},
	{ 0xff, 0x76, 0x00, NULL,	"Exception exit"	},
	{ 0xff, 0x7e, 0x00, NULL,	"Exception entry"	},
	{ 0xff, 0x72, 0x00, NULL,	"Reserved"			},
	{ 0x81, 0x80, 0x00, pheader,	"P-header"		},

	{ 0x00, 0x00, 0x00, NULL,	"Unknown"			},
};

struct packet_type packet_types_pft[] = {
	{ 0xff, 0x00, 0x00, async,	"A-sync"				},
	{ 0xff, 0x08, 0x00, isync_pft,	"I-sync"			},
	{ 0x81, 0x80, 0x00, atom,	"Atom"					},
	{ 0x01, 0x01, 0x00, branch_pft,	"Branch address"	},
	{ 0xff, 0x72, 0x00, waypoint,	"Waypoint update"	},
	{ 0xff, 0x0c, 0x00, trigger,	"Trigger"			},
	{ 0xff, 0x6e, 0x00, contextid,	"Context ID"		},
	{ 0xff, 0x3c, 0x00, vmid,	"VMID"					},
	{ 0xfb, 0x42, 0x00, timestamp_pft, "Timestamp"		},
	{ 0xff, 0x76, 0x00, except_ret,	"Exception return"	},
	{ 0xff, 0x66, 0x00, ignore,	"Ignore"				},

	{ 0xff, 0x04, 0x00, NULL,	"Reserved"			},
	{ 0x93, 0x00, 0x60, NULL,	"Reserved"			},
	{ 0xff, 0x50, 0x00, NULL,	"Reserved"			},
	{ 0xff, 0x70, 0x00, NULL,	"Reserved"			},
	{ 0xd3, 0x50, 0x0c, NULL,	"Reserved"			},
	{ 0xd3, 0x12, 0x00, NULL,	"Reserved"			},
	{ 0xf3, 0x10, 0x00, NULL,	"Reserved"			},
	{ 0xfb, 0x30, 0x00, NULL,	"Reserved"			},
	{ 0xff, 0x38, 0x00, NULL,	"Reserved"			},
	{ 0xd3, 0x02, 0x00, NULL,	"Reserved"			},
	{ 0xf3, 0x52, 0x00, NULL,	"Reserved"			},
	{ 0xfb, 0x4a, 0x00, NULL,	"Reserved"			},
	{ 0xff, 0x62, 0x00, NULL,	"Reserved"			},
	{ 0xef, 0x6a, 0x00, NULL,	"Reserved"			},
	{ 0xff, 0x7e, 0x00, NULL,	"Reserved"			},

	{ 0x00, 0x00, 0x00, NULL,	"Unknown"			},
};

struct packet_type *packet_types;

int is_match(int i, int ch)
{
	//print_state(state, "is_match");
	return (ch & packet_types[i].match_mask) == packet_types[i].match_val &&
		(!packet_types[i].nonzero_mask ||
		 (packet_types[i].nonzero_mask & ch));
}

int main(int argc, char **argv)
{	
	static int index;
	char *input_file_name = NULL;//trace/alea_trace.bin
	int ch;
	int i;
	int mc;
	int (*decode)(struct state *state);
	bool print_config = false;
	struct state state;
	int c, ret;
	int option_index = 0;

	enum options {
		OPT_ETM_3_3,
		OPT_ETM_3_4_ALT,
		OPT_PFT_1_1,
		OPT_CYCLE_ACCURATE,
		OPT_CONTEXTID_BYTES,
		OPT_FORMATTER,
		OPT_SOURCEID,
		OPT_LONG_WAIT,
		OPT_PRINT_INPUT,
		OPT_PRINT_CONFIG,
		OPT_PRINT_HELP,
		OPT_INPUT_FILE,
	};

	struct option long_options[] = {
		[OPT_ETM_3_3] = { "etm-3.3", 0, 0, 0 },
		[OPT_ETM_3_4_ALT] ={ "etm-3.4-alt-branch", 0, 0, 0 },
		[OPT_PFT_1_1] = { "pft-1.1", 0, 0, 0 },
		[OPT_CYCLE_ACCURATE] = { "cycle-accurate", 2, &state.cycle_accurate, false },
		[OPT_CONTEXTID_BYTES] = { "contextid-bytes", 1, &state.contextid_bytes, 0 },
		[OPT_FORMATTER] = { "formatter", 2, &state.formatter, true },
		[OPT_SOURCEID] = { "sourceid-match", 1, 0, 0 },
		[OPT_LONG_WAIT] = { "print-long-waits", 1, (int*)&state.long_wait, 0 },
		[OPT_PRINT_INPUT] = { "print-input", 2, &state.print_input, true },
		[OPT_PRINT_CONFIG] = { "print-config", 0, &print_config, true },
		[OPT_PRINT_HELP] = { "help", 0, 0, 'h'},
		[OPT_INPUT_FILE] = { "input", 1, 0,0},
		{},
	};
	const char *help_txt[] = {
		[OPT_ETM_3_3] = "ETM v3.3 trace data",
		[OPT_ETM_3_4_ALT] = "ETM v3.4 trace data with alternative branch encoding",
		[OPT_PFT_1_1] = "PFT v1.1 trace data\n",

		[OPT_CYCLE_ACCURATE] = "Cycle-accurate tracing was enabled (Default 1)",
		[OPT_CONTEXTID_BYTES] = "Number of Context ID bytes (Default 4)\n",

		[OPT_FORMATTER] = "Enable Formatter Unpacking",
		[OPT_SOURCEID] = "Enable Source ID from formatter. Also enables formatter\n",

		[OPT_LONG_WAIT] = "Highlight long waits",
		[OPT_PRINT_INPUT] = "Print input data",
		[OPT_PRINT_CONFIG] = "Print configuration data",
		[OPT_PRINT_HELP] = "Print usage information",
		[OPT_INPUT_FILE] = "Input file (.bin)",
	};

	memset(&state, 0, sizeof(state));
	state.contextid_bytes = 0;
//	state.cycle_accurate = 1;
	state.long_wait = 1000000;

	while (1) {
		c = getopt_long(argc, argv, "h", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 0:
			switch (option_index) {
			case OPT_ETM_3_3:
				state.alt_branch = false;
				packet_types = packet_types_etm_3_3;
				break;
			case OPT_ETM_3_4_ALT:
				state.alt_branch = true;
				packet_types = packet_types_etm_3_3;
				break;
			case OPT_PFT_1_1:
				state.alt_branch = true;
				state.program_flow_only = true;
				packet_types = packet_types_pft;
				break;
			case OPT_SOURCEID:
				state.formatter = true;
				state.sourceid_match |= 1 << atoi(optarg);
				break;
			case OPT_INPUT_FILE: 
				input_file_name = optarg;
				//printf("input_file_name = %s\n", input_file_name);				
				break;
			default: {
				int *flag = long_options[option_index].flag;
				if (optarg && flag)
					*flag = atoi(optarg);
				} break;
			};
			if (print_config) {
				printf("option %s", long_options[option_index].name);
				if (optarg)
					printf(" with arg %s", optarg);
				printf("\n");
			}
			break;

		case 'h':
			printf("Usage: %s [options]\n", argv[0]);
			printf("Options:\n");
			for (i = 0; long_options[i].name; i++) {
				printf("  --%-20s %s\n", long_options[i].name, help_txt[i]);
			}
			return 0;

		case '?':
			return 1;

		default:
			printf("unhandled getopt return code %d\n", c);
			return 1;
		}
	}
	if (packet_types == NULL) {
		printf("%s: Must specify etm/pft type\n", argv[0]);
		return 1;
	}
	if (print_config) {
		printf("contextid_bytes %d\n", state.contextid_bytes);
		printf("cycle_accurate %d\n", state.cycle_accurate);
		printf("alt_branch %d\n", state.alt_branch);
		printf("formatter %d\n", state.formatter);
		printf("sourceid_match 0x%x\n", state.sourceid_match);
		printf("print_input %d\n", state.print_input);
		printf("long_wait %d\n", state.long_wait);
	}

	struct stat sb;	
		ret = stat(input_file_name, &sb);
   	if (ret == -1) {
       	LOGE("Cannot stat %s (%s)\n", input_file_name, strerror(errno));
       	return EXIT_FAILURE;
   	}

	int SIZE = sb.st_size;	
	//printf("SIZE = %d\n", SIZE);

	FILE *file_ptr;

	buffer = malloc(SIZE*sizeof(char));
	if (buffer == NULL)
		printf("Cannot allocate mempry space for buffer\n");
	else
	{
		// sizeof(buffer) renvoie 8 => à sauvegarder la taille manuellement
		//printf("buffer size = %d\n", SIZE);

		file_ptr = fopen(input_file_name,"r"); 
		if (file_ptr == NULL)
		{
			printf("Cannot open file %s\n", input_file_name);
		}
		else
		{
			ret = fread(buffer, 1, SIZE, file_ptr);						
			fclose(file_ptr);

			#ifdef DISPLAY_DEBUG
			int idx;
			printf("Read %u bytes\n", ret);
			printf("Buffer \n");
			for (idx = 0; idx < SIZE; idx++)
			{
				printf("%01x ", buffer[idx]);
				if (idx % 16 == 0 && idx != 0)
					printf("\n");
			}
			#endif
		}
	}

	for (ch = 0; ch < 256; ch++) {
		mc = 0;
		for (i = 0; packet_types[i].match_mask; i++)
			if (is_match(i, ch))
				mc++;
		if (mc == 1)
			continue;
		if (mc == 0)
			printf("%02x not covered\n", ch);
		else {
			printf("%02x has multiple matches, %d\n", ch, mc);
			for (i = 0; packet_types[i].match_mask; i++)
				if (is_match(i, ch))
					printf("    %02x %02x %02x %s\n", packet_types[i].match_mask,
					packet_types[i].match_val, packet_types[i].nonzero_mask,
					packet_types[i].name);
		}
	}

	while (1) {
		ch = get_byte(&state);

		for (i = 0; !is_match(i, ch); i++);
		if (state.print_input)
			printf("  %s\n", packet_types[i].name);
		
		decode = packet_types[i].decode;
		if (decode) {
			decode(&state);
		} else {
			state.sync = 0;
			printf("  %02x: not handled (%s)\n", ch, packet_types[i].name);
		}
		index++;
		if (index == SIZE)
			break;
	}
	return 0;
}
