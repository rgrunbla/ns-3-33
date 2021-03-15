/******************************************************************************
 *
 * Copyright(c) 2005 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 * Copyright(c) 2016 - 2017 Intel Deutschland GmbH
 * Copyright(c) 2018 - 2019 Intel Corporation
 * Copyright(c) 2019 - Rémy Grünblatt <remy@grunblatt.org>
 *
 * Contact Information:
 * Rémy Grünblatt <remy@grunblatt.org>
 *
 *****************************************************************************/
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/random-variable-stream.h"
#include "intel-rate-wifi-manager.h"
#include "wifi-tx-vector.h"
#include "wifi-utils.h"
#include "wifi-mac.h"
#include "wifi-phy.h"

#include <iomanip>
#include <iostream>
#include <algorithm>
#include <set>
#include <vector>
#include <utility>
#include <map>

#define INVALID_THROUGHPUT                     -1
#define INVALID_INDEX                          -1

#define IWL_MVM_RS_RATE_MIN_FAILURE_TH          3
#define IWL_MVM_RS_RATE_MIN_SUCCESS_TH          8
#define IWL_MVM_RS_SR_FORCE_DECREASE           15
#define IWL_MVM_RS_SR_NO_DECREASE              85
#define IWL_MVM_RS_STAY_IN_COLUMN_TIMEOUT       5

#define IWL_MVM_RS_LEGACY_FAILURE_LIMIT       160
#define IWL_MVM_RS_LEGACY_SUCCESS_LIMIT       480
#define IWL_MVM_RS_LEGACY_TABLE_COUNT         160
#define IWL_MVM_RS_NON_LEGACY_FAILURE_LIMIT   400
#define IWL_MVM_RS_NON_LEGACY_SUCCESS_LIMIT  4500
#define IWL_MVM_RS_NON_LEGACY_TABLE_COUNT    1500
#define DEBUG false

/* Right now, the algorithm only supports up to 3 antennas. It's more a limit of
   the Intel hardware, and might be extended in the future to support more
   antennas */
enum antenna { A, B, C};

/* Intel can either use a LEGACY transmission mode (802.11a or 802.11g), or a
   non-legacy transmission mode (SISO if you have one spatial stream, or MIMO if
   you have multiple spatial steams). The intel driver only supports 2 spatial
   streams */
enum column_mode { LEGACY, SISO, MIMO };

enum rate_type { NONE, LEGACY_G };

/* Guard Interval Duration */
enum guard_interval { SGI = 0, LGI = 1 };

/* MCS Scaling actions (decreasing the MCS index, maintaining the MCS index, or
   increasing the MCS index). */
enum rs_action { RS_ACTION_STAY = 0, RS_ACTION_DOWNSCALE = -1, RS_ACTION_UPSCALE = 1 };

/* Bandwidth */
enum bandwidth { BW_20 = 20, BW_40 = 40, BW_80 = 80, BW_160 = 160 };

/* Whether AMPDU aggregation is enabled or not */
enum aggregation { NO_AGG = 0, AGG = 1 };

enum state { RS_STATE_SEARCH_CYCLE_STARTED, RS_STATE_SEARCH_CYCLE_ENDED, RS_STATE_STAY_IN_COLUMN };

#define RS_PERCENT(x) (128 * x)

class History {
	std::vector<bool> data;
	int max_throughput = INVALID_THROUGHPUT;

	public:
	History(int max_throughput) {
		this->set_max_throughput(max_throughput);
	}
	History() {
	}

	int get_max_throughput() {
		return this->max_throughput;
	}

	void set_max_throughput(int max_throughput) {
		this->max_throughput = max_throughput;
	}

	void reset() {
		(this->data).clear();
	}

	int counter() {
		return this->data.size();
	}

	int average_tpt() {
		if ((this->fail_counter() >= IWL_MVM_RS_RATE_MIN_FAILURE_TH) || (this->success_counter() >= IWL_MVM_RS_RATE_MIN_SUCCESS_TH)) {
			return (this->success_ratio() * this->get_max_throughput() + 64) / 128;
		} else {
			return INVALID_THROUGHPUT;
		}
	}

	int success_counter() {
		if (this->data.empty()) {
			return INVALID_THROUGHPUT;
		}
		return std::count(this->data.begin(), this->data.end(), 1);
	}

	int fail_counter() {
		if(this->data.empty()) {
			return INVALID_THROUGHPUT;
		}
		return std::count(this->data.begin(), this->data.end(), 0);
	}

	void tx(bool success) {
		this->data.emplace(this->data.begin(), success);
		if (this->data.size() > 62) {
			this->data.resize(62);
		}
	}

	int success_ratio() {
		if (this->counter() > 0) {
			return 128 * 100 * this->success_counter() / this->counter();
		} else {
			return -1;
		}
	}

	void print() {
		if (DEBUG) {
			std::cout << (double) this->success_ratio() / (128*100) << " % (" << this->success_ratio() << " / " << 128*100 << "), " << this->average_tpt() << ", ";
			for(auto i: this->data) {
				std::cout << (int) i;
			}
			std::cout << "\n";
		}
	}

};

class TheoreticalThroughputTable {
	public:
		column_mode mode;
		bandwidth bw;
		guard_interval gi;
		aggregation agg;
		std::vector<int> throughputs;

		bool match(column_mode mode, bandwidth bw, guard_interval gi, aggregation agg) {
			return ((mode == this->mode) && (bw == this -> bw) && (gi == this->gi) && (agg == this->agg));
		}

		TheoreticalThroughputTable(column_mode mode, bandwidth bw, guard_interval gi, aggregation agg, std::vector<int> throughputs) {
			this->mode = mode;
			this->bw = bw;
			this->gi = gi;
			this->agg = agg;
			this->throughputs = throughputs;
		}
};

std::map<std::tuple<column_mode, bandwidth, guard_interval, aggregation>, std::vector<int>> buildTheoreticalThroughputTables() {
	std::map<std::tuple<column_mode, bandwidth, guard_interval, aggregation>, std::vector<int>> TheoreticalThroughputTables;

	/* These tables represent the theoretical throughput and has been taken from
	   the Intel Driver Source Code. Due to this fact, the license of this code
	   has to be the same as the intel driver.

	   Each table encodes the maximum theoretical throughput for each combination
	   of column mode (LEGACY, SISO, MIMO), bandwidth (20MHz, 40Mhz, 80Mhz,
	   160Mhz), guard interval duration (Long or Short), and AMPDU aggregation.
	   */

	// expected_tpt_LEGACY
	TheoreticalThroughputTables[{LEGACY, BW_20, LGI, NO_AGG}] = {7, 13, 35, 58, 40, 57, 72, 98, 121, 154, 177, 186, 0, 0, 0};

	// expected_tpt_SISO_20MHz
	TheoreticalThroughputTables[{SISO, BW_20, LGI, NO_AGG}] =  {0, 0, 0, 0, 42, 0, 76, 102, 124, 159, 183, 193, 202, 216, 0};
	TheoreticalThroughputTables[{SISO, BW_20, SGI, NO_AGG}] =  {0, 0, 0, 0, 46, 0, 82, 110, 132, 168, 192, 202, 210, 225, 0};
	TheoreticalThroughputTables[{SISO, BW_20, LGI, AGG}] =  {0, 0, 0, 0, 49, 0, 97, 145, 192, 285, 375, 420, 464, 551, 0};
	TheoreticalThroughputTables[{SISO, BW_20, SGI, AGG}] =  {0, 0, 0, 0, 54, 0, 108, 160, 213, 315, 415, 465, 513, 608, 0};

	// expected_tpt_SISO_40MHz
	TheoreticalThroughputTables[{SISO, BW_40, LGI, NO_AGG}] =  {0, 0, 0, 0, 77, 0, 127, 160, 184, 220, 242, 250, 257, 269, 275};
	TheoreticalThroughputTables[{SISO, BW_40, SGI, NO_AGG}] =  {0, 0, 0, 0, 83, 0, 135, 169, 193, 229, 250, 257, 264, 275, 280};
	TheoreticalThroughputTables[{SISO, BW_40, LGI, AGG}] =  {0, 0, 0, 0, 101, 0, 199, 295, 389, 570, 744, 828, 911, 1070, 1173};
	TheoreticalThroughputTables[{SISO, BW_40, SGI, AGG}] =  {0, 0, 0, 0, 112, 0, 220, 326, 429, 629, 819, 912, 1000, 1173, 1284};

	// expected_tpt_SISO_80MHz
	TheoreticalThroughputTables[{SISO, BW_80, LGI, NO_AGG}] =  {0, 0, 0, 0, 130, 0, 191, 223, 244, 273, 288, 294, 298, 305, 308};
	TheoreticalThroughputTables[{SISO, BW_80, SGI, NO_AGG}] =  {0, 0, 0, 0, 138, 0, 200, 231, 251, 279, 293, 298, 302, 308, 312};
	TheoreticalThroughputTables[{SISO, BW_80, LGI, AGG}] =  {0, 0, 0, 0, 217, 0, 429, 634, 834, 1220, 1585, 1760, 1931, 2258, 2466};
	TheoreticalThroughputTables[{SISO, BW_80, SGI, AGG}] =  {0, 0, 0, 0, 241, 0, 475, 701, 921, 1343, 1741, 1931, 2117, 2468, 2691};

	// expected_tpt_SISO_160MHz
	TheoreticalThroughputTables[{SISO, BW_160, LGI, NO_AGG}] =  {0, 0, 0, 0, 191, 0, 244, 288, 298, 308, 313, 318, 323, 328, 330};
	TheoreticalThroughputTables[{SISO, BW_160, SGI, NO_AGG}] =  {0, 0, 0, 0, 200, 0, 251, 293, 302, 312, 317, 322, 327, 332, 334};
	TheoreticalThroughputTables[{SISO, BW_160, LGI, AGG}] =  {0, 0, 0, 0, 439, 0, 875, 1307, 1736, 2584, 3419, 3831, 4240, 5049, 5581};
	TheoreticalThroughputTables[{SISO, BW_160, SGI, AGG}] =  {0, 0, 0, 0, 488, 0, 972, 1451, 1925, 2864, 3785, 4240, 4691, 5581, 6165};

	// expected_tpt_MIMO2_20MHz
	TheoreticalThroughputTables[{MIMO, BW_20, LGI, NO_AGG}] =  {0, 0, 0, 0, 74, 0, 123, 155, 179, 213, 235, 243, 250, 261, 0};
	TheoreticalThroughputTables[{MIMO, BW_20, SGI, NO_AGG}] =  {0, 0, 0, 0, 81, 0, 131, 164, 187, 221, 242, 250, 256, 267, 0};
	TheoreticalThroughputTables[{MIMO, BW_20, LGI, AGG}] =  {0, 0, 0, 0, 98, 0, 193, 286, 375, 550, 718, 799, 878, 1032, 0};
	TheoreticalThroughputTables[{MIMO, BW_20, SGI, AGG}] =  {0, 0, 0, 0, 109, 0, 214, 316, 414, 607, 790, 879, 965, 1132, 0};

	// expected_tpt_MIMO2_40MHz
	TheoreticalThroughputTables[{MIMO, BW_40, LGI, NO_AGG}] =  {0, 0, 0, 0, 123, 0, 182, 214, 235, 264, 279, 285, 289, 296, 300};
	TheoreticalThroughputTables[{MIMO, BW_40, SGI, NO_AGG}] =  {0, 0, 0, 0, 131, 0, 191, 222, 242, 270, 284, 289, 293, 300, 303};
	TheoreticalThroughputTables[{MIMO, BW_40, LGI, AGG}] =  {0, 0, 0, 0, 200, 0, 390, 571, 741, 1067, 1365, 1505, 1640, 1894, 2053};
	TheoreticalThroughputTables[{MIMO, BW_40, SGI, AGG}] =  {0, 0, 0, 0, 221, 0, 430, 630, 816, 1169, 1490, 1641, 1784, 2053, 2221};

	// expected_tpt_MIMO2_80MHz
	TheoreticalThroughputTables[{MIMO, BW_80, LGI, NO_AGG}] =  {0, 0, 0, 0, 182, 0, 240, 264, 278, 299, 308, 311, 313, 317, 319};
	TheoreticalThroughputTables[{MIMO, BW_80, SGI, NO_AGG}] =  {0, 0, 0, 0, 190, 0, 247, 269, 282, 302, 310, 313, 315, 319, 320};
	TheoreticalThroughputTables[{MIMO, BW_80, LGI, AGG}] =  {0, 0, 0, 0, 428, 0, 833, 1215, 1577, 2254, 2863, 3147, 3418, 3913, 4219};
	TheoreticalThroughputTables[{MIMO, BW_80, SGI, AGG}] =  {0, 0, 0, 0, 474, 0, 920, 1338, 1732, 2464, 3116, 3418, 3705, 4225, 4545};

	// expected_tpt_MIMO2_160MHz
	TheoreticalThroughputTables[{MIMO, BW_160, LGI, NO_AGG}] =  {0, 0, 0, 0, 240, 0, 278, 308, 313, 319, 322, 324, 328, 330, 334};
	TheoreticalThroughputTables[{MIMO, BW_160, SGI, NO_AGG}] =  {0, 0, 0, 0, 247, 0, 282, 310, 315, 320, 323, 325, 329, 332, 338};
	TheoreticalThroughputTables[{MIMO, BW_160, LGI, AGG}] =  {0, 0, 0, 0, 875, 0, 1735, 2582, 3414, 5043, 6619, 7389, 8147, 9629, 10592};
	TheoreticalThroughputTables[{MIMO, BW_160, SGI, AGG}] =  {0, 0, 0, 0, 971, 0, 1925, 2861, 3779, 5574, 7304, 8147, 8976, 10592, 11640};

	return TheoreticalThroughputTables;
}

class Column {
	public:
		column_mode mode = LEGACY;
		std::set<antenna> antennas = {};
		guard_interval gi = LGI;
		std::vector<std::tuple<column_mode, std::set<antenna>, guard_interval>> next_columns;
		Column() {}

		Column(column_mode mode, std::set<antenna> antennas, guard_interval gi) {
			this->mode = mode;
			this->antennas = antennas;
			this->gi = gi;
		}

		std::tuple<column_mode, std::set<antenna>, guard_interval> getColumn() {
			return {this->mode, this->antennas, this->gi};
		}

		void setNextColumns(std::vector<Column> columns) {
			for (Column col: columns) {
				this->next_columns.push_back(col.getColumn());
			}
		}

		std::vector<std::tuple<column_mode, std::set<antenna>, guard_interval>> getNextColumns() {
			return this->next_columns;
		}
};


std::map<std::tuple<column_mode, std::set<antenna>, guard_interval>, Column> buildColumns() {
	std::map<std::tuple<column_mode, std::set<antenna>, guard_interval>, Column> columns;
	Column LEGACY_ANT_A(LEGACY, {A}, LGI);
	Column LEGACY_ANT_B(LEGACY, {B}, LGI);
	Column SISO_ANT_A(SISO, {A}, LGI);
	Column SISO_ANT_B(SISO, {B}, LGI);
	Column SISO_ANT_A_SGI(SISO, {A}, SGI);
	Column SISO_ANT_B_SGI(SISO, {B}, SGI);
	Column MIMO2(MIMO, {A, B}, LGI);
	Column MIMO2_SGI(MIMO, {A, B}, SGI);
	LEGACY_ANT_A.setNextColumns({LEGACY_ANT_B, SISO_ANT_A, MIMO2});
	LEGACY_ANT_B.setNextColumns({LEGACY_ANT_A, SISO_ANT_B, MIMO2});
	SISO_ANT_A.setNextColumns({SISO_ANT_B, MIMO2, SISO_ANT_A_SGI, LEGACY_ANT_A, LEGACY_ANT_B});
	SISO_ANT_B.setNextColumns({SISO_ANT_A, MIMO2, SISO_ANT_B_SGI, LEGACY_ANT_A, LEGACY_ANT_B});
	SISO_ANT_A_SGI.setNextColumns({SISO_ANT_B_SGI, MIMO2_SGI, SISO_ANT_A, LEGACY_ANT_A, LEGACY_ANT_B});
	SISO_ANT_B_SGI.setNextColumns({SISO_ANT_A_SGI, MIMO2_SGI, SISO_ANT_B, LEGACY_ANT_A, LEGACY_ANT_B});
	MIMO2.setNextColumns({SISO_ANT_A, MIMO2_SGI, LEGACY_ANT_A, LEGACY_ANT_B});
	MIMO2_SGI.setNextColumns({SISO_ANT_A_SGI, MIMO2, LEGACY_ANT_A, LEGACY_ANT_B});
	columns[{LEGACY, {A}, LGI}] = LEGACY_ANT_A;
	columns[{LEGACY, {B}, LGI}] = LEGACY_ANT_B;
	columns[{SISO, {A}, LGI}] = SISO_ANT_A;
	columns[{SISO, {B}, LGI}] = SISO_ANT_B;
	columns[{SISO, {A}, SGI}] = SISO_ANT_A_SGI;
	columns[{SISO, {B}, SGI}] = SISO_ANT_B_SGI;
	columns[{MIMO, {A,B}, LGI}] = MIMO2;
	columns[{MIMO, {A,B}, SGI}] = MIMO2_SGI;

	return columns;
}

class State {
	public:
		bool columnScaling = false;
		int last_tpt = 0;
		int index = 0;
		column_mode mode = LEGACY;
		rate_type type = LEGACY_G;
		bandwidth bandWidth = BW_20;
		bandwidth maxWidth = BW_20;
		guard_interval guardInterval = LGI;
		aggregation agg = NO_AGG;
		std::set<antenna> antennas = {A};
		state s = RS_STATE_SEARCH_CYCLE_STARTED;

		int total_failed = 0;
		int total_success = 0;
		int table_count = 0;
		int64_t flush_timer = 0;
		int64_t last_tx = 0;


		/* Columns already visted during the search cycle. Should never be
		   empty, as the current column is being visited. */
		std::set<std::tuple<column_mode, std::set<antenna>, guard_interval>> visited_columns;

		/* When trying a new column, this field stores the parameters of the old
		   column so that in case the new column is not so good, we can go back
		   to the old column */
		std::tuple<column_mode, std::set<antenna>, guard_interval, int, bandwidth> oldColumnParameters;

		/* Maximum theoretical throughput for each MCS for different parameters
		   of aggregation (AMPDU), guard interval, bandwidth, and column mode.
		   Extracted from the driver source code */
		std::map<std::tuple<column_mode, bandwidth, guard_interval, aggregation>, std::vector<int>> TheoreticalThroughputTables;

		/* History for each rate for each parameters. In the original driver,
		   only the history of the current column and the search column are
		   saved, we emulate this behaviour by emptying these histories but it's
		   easier to maintain a map than switching between two tables all the
		   time. */
		std::map<std::tuple<column_mode, bandwidth, guard_interval, aggregation, int>, History> Histories;

		// Available Columns
		std::map<std::tuple<column_mode, std::set<antenna>, guard_interval>, Column> columns;

		State () {}

		State (int maxWidth) {
			this->TheoreticalThroughputTables = buildTheoreticalThroughputTables();
			for (auto const& TheoreticalThroughputTable: this->TheoreticalThroughputTables) {
				for(uint i=0; i<TheoreticalThroughputTable.second.size(); ++i) {
					this->Histories[std::tuple_cat(TheoreticalThroughputTable.first, std::make_tuple(i))] = History(TheoreticalThroughputTable.second[i]);
				}
			}
			this->columns = buildColumns();
			this->visited_columns.insert({LEGACY, {A}, LGI});
			if (maxWidth == 20) {
				this->maxWidth = BW_20;
			} else if (maxWidth == 40) {
				this->maxWidth = BW_40;
			} else if (maxWidth == 80) {
				this->maxWidth = BW_80;
			} else {
				this->maxWidth = BW_160;
			}
		}

		void printHistory() {
			this->getHistory()->print();
		}

		History * getHistory() {
			return &(this->Histories[std::make_tuple(this->mode, this->bandWidth, this->guardInterval, this->agg, this->index)]);
		}

		History * getHistory(int index) {
			return &(this->Histories[std::make_tuple(this->mode, this->bandWidth, this->guardInterval, this->agg, index)]);
		}

		void clearHistories() {
			for(int i=0; i<15; ++i) {
				this->getHistory(i)->reset();
			}
		}

		Column * getColumn() {
			return &(this->columns[{this->mode, this->antennas, this->guardInterval}]);
		}

		Column * getColumn(column_mode mode, std::set<antenna> antennas, guard_interval gi) {
			return &(this->columns[{mode, antennas, gi}]);

		}

		int getMaxSuccessLimit() {
			if (this->mode == LEGACY) {
				return IWL_MVM_RS_LEGACY_SUCCESS_LIMIT;
			} else {
				return IWL_MVM_RS_NON_LEGACY_SUCCESS_LIMIT;
			}
		}

		int getMaxFailureLimit() {
			if (this->mode == LEGACY) {
				return IWL_MVM_RS_LEGACY_FAILURE_LIMIT;
			} else {
				return IWL_MVM_RS_NON_LEGACY_FAILURE_LIMIT;
			}
		}

		int getTableCountLimit() {
			if (this->mode == LEGACY) {
				return IWL_MVM_RS_LEGACY_TABLE_COUNT;
			} else {
				return IWL_MVM_RS_NON_LEGACY_TABLE_COUNT;
			}
		}

		void set_stay_in_table() {
			if (DEBUG) std::cout << "Moving to RS_STATE_STAY_IN_COLUMN\n";
			this->s = RS_STATE_STAY_IN_COLUMN;
			this->total_failed = 0;
			this->total_success = 0;
			this->table_count = 0;
			this->flush_timer = ns3::Simulator::Now().GetNanoSeconds();
			this->visited_columns = {std::make_tuple(this->mode, this->antennas, this->guardInterval)};
		}

		void stay_in_table() {
			if (this->s == RS_STATE_STAY_IN_COLUMN) {
				bool flush_interval_passed = false;
				if (this->flush_timer) flush_interval_passed = ((ns3::Simulator::Now().GetNanoSeconds() - this->flush_timer) >= (5000000000*IWL_MVM_RS_STAY_IN_COLUMN_TIMEOUT));

				if((this->total_failed > this->getMaxFailureLimit()) || (this->total_success > this->getMaxSuccessLimit()) || ((!this->columnScaling) && flush_interval_passed)) {
					if (DEBUG) std::cout << "LQ: stay is expired " << (this->total_failed > this->getMaxFailureLimit()) << " " << (this->total_success > this->getMaxSuccessLimit()) << " " << (!this->columnScaling) << " " << flush_interval_passed << "\n";
					this->s = RS_STATE_SEARCH_CYCLE_STARTED;
					this->total_failed = 0;
					this->total_success = 0;
					this->table_count = 0;
					this->flush_timer = 0;
					this->visited_columns = {std::make_tuple(this->mode, this->antennas, this->guardInterval)};
				} else {
					this->table_count++;
					if (this->table_count > this->getTableCountLimit()) {
						this->table_count = 0;
						if (DEBUG) std::cout << "LQ: stay in table. Clear the histories.\n";
						this->clearHistories();
					}
				}
			}
		}

		std::tuple<int, int> getAdjacentRatesIndexes() {
			int max_index = 14;
			if ((this->bandWidth == 20) && (this->mode != LEGACY)) {
				max_index = 13;
			}

			std::tuple<int, int> indexes = {INVALID_INDEX, INVALID_INDEX};
			if (this->type != LEGACY_G) {
				for(int i=this->index-1; i>INVALID_INDEX; --i) {
					if(this->getHistory(i)->get_max_throughput() != 0) {
						std::get<0>(indexes) = i;
						break;
					}
				}
				for(int i=this->index+1; i<=max_index; ++i) {
					if(this->getHistory(i)->get_max_throughput() != 0) {
						std::get<1>(indexes) = i;
						break;
					}
				}
			} else {
				std::vector<std::tuple<int, int>> legacy_g_mapping = {
					{-1, 1},
					{0, 2},
					{1, 3},
					{5, 6},
					{2, 3},
					{4, 3},
					{3, 7},
					{6, 8},
					{7, 9},
					{8, 10},
					{9, 11},
					{10, -1}
				};
				if (DEBUG) std::cout << "this->index: " << this->index << "\n\n";
				return legacy_g_mapping[this->index];
			}
			return indexes;
		}

		rs_action MCSScaling(std::tuple<int, int> adjacentIndexes, std::tuple<int, int> adjacentRates) {
			rs_action action = RS_ACTION_STAY;

			if ((this->getHistory()->success_ratio() <= RS_PERCENT(IWL_MVM_RS_SR_FORCE_DECREASE)) ||
					(this->getHistory()->average_tpt() == 0)) {
				if (DEBUG) std::cout << "Decrease rate because of low SR\n";
				return RS_ACTION_DOWNSCALE;
			}

			if ((std::get<0>(adjacentRates) == INVALID_THROUGHPUT) &&
					(std::get<1>(adjacentRates) == INVALID_THROUGHPUT) &&
					(std::get<1>(adjacentIndexes) != INVALID_INDEX)) {
				if (DEBUG) std::cout << "No data about high/low rates. Increase rate\n";
				return RS_ACTION_UPSCALE;
			}

			if ((std::get<1>(adjacentRates) == INVALID_THROUGHPUT) &&
					(std::get<1>(adjacentIndexes) != INVALID_INDEX) &&
					(std::get<0>(adjacentRates) != INVALID_THROUGHPUT) &&
					(std::get<0>(adjacentRates) < this->getHistory()->average_tpt())) {
				if (DEBUG) std::cout << "No data about high rate and low rate is worse. Increase rate\n";
				return RS_ACTION_UPSCALE;
			}

			if ((std::get<1>(adjacentRates) != INVALID_THROUGHPUT) &&
					(std::get<1>(adjacentRates) > this->getHistory()->average_tpt())) {
				if (DEBUG) std::cout << "Higher rate is better. Increate rate\n";
				return RS_ACTION_UPSCALE;
			}

			if ((std::get<0>(adjacentRates) != INVALID_THROUGHPUT) &&
					(std::get<1>(adjacentRates) != INVALID_THROUGHPUT) &&
					(std::get<0>(adjacentRates) < this->getHistory()->average_tpt()) &&
					(std::get<1>(adjacentRates) < this->getHistory()->average_tpt())) {
				if (DEBUG) std::cout << "Both high and low are worse. Maintain rate\n";
				return RS_ACTION_STAY;
			}

			if ((std::get<0>(adjacentRates) != INVALID_THROUGHPUT) &&
					(std::get<0>(adjacentRates) > this->getHistory()->average_tpt())) {
				if (DEBUG) std::cout << "Lower rate is better\n";
				action = RS_ACTION_DOWNSCALE;
			} else if ((std::get<0>(adjacentRates) == INVALID_THROUGHPUT) &&
					(std::get<0>(adjacentIndexes) != INVALID_INDEX)) {
				if (DEBUG) std::cout << "No data about lower rate\n";
				action = RS_ACTION_DOWNSCALE;
			} else {
				if (DEBUG) std::cout << "Maintain rate\n";
			}

			if ((action == RS_ACTION_DOWNSCALE) && (std::get<0>(adjacentIndexes) != INVALID_INDEX)) {
				if (this->getHistory()->success_ratio() >= RS_PERCENT(IWL_MVM_RS_SR_NO_DECREASE)) {
					if (DEBUG) std::cout << "SR is above NO DECREASE. Avoid downscale\n";
					action = RS_ACTION_STAY;
				} else if (this->getHistory()->average_tpt() > (100 * this->getHistory(std::get<0>(adjacentIndexes))->get_max_throughput())) {
					if (DEBUG) std::cout << "Current TPT is higher than max expected in low rate. Avoid downscale\n";
					action = RS_ACTION_STAY;
				} else {
					if (DEBUG) std::cout << "Decrease rate\n";
				}
			}
			return action;
		}

		std::tuple<bool, std::tuple<column_mode, std::set<antenna>, guard_interval>> getNextColumn(std::set<std::tuple<column_mode, std::set<antenna>, guard_interval>> visited_columns) {
			if (DEBUG) std::cout << "Visited columns: " << visited_columns.size() << "\n";
			for (auto key: this->getColumn()->next_columns) {
				if (visited_columns.find(key) == visited_columns.end()) {
					bandwidth bandWidth = this->bandWidth;
					if (std::get<0>(key) == LEGACY) {
						bandWidth = BW_20;
					}
					// Check Throughput can be beaten
					std::vector<int> throughputs = this->TheoreticalThroughputTables[std::make_tuple(std::get<0>(key), bandWidth, std::get<2>(key), this->agg)];
					int max_expected_tpt = *std::max_element(throughputs.begin(), throughputs.end());

					if ((100*max_expected_tpt) <= this->getHistory()->average_tpt()) {
						if (DEBUG) std::cout << "Skip column: can't beat current TPT. Max expected " << (max_expected_tpt*100) << " current " << this->getHistory()->average_tpt() << "\n";
						continue;
					}

					return {true, key};
				}
			}
			return {false, {LEGACY, {A}, LGI}};
		}

		int getNextIndex(std::tuple<column_mode, std::set<antenna>, guard_interval> newColumnParameters) {
			int throughputThreshold = INVALID_THROUGHPUT;
			int newIndex = INVALID_INDEX;
			if (this->getHistory()->success_ratio() >= RS_PERCENT(IWL_MVM_RS_SR_NO_DECREASE)) {
				throughputThreshold = this->getHistory()->get_max_throughput()*100;
				if (DEBUG) std::cout << "SR " << this->getHistory()->success_ratio() << " high. Find rate exceeding EXPECTED_CURRENT " << throughputThreshold << "\n";
			} else {
				throughputThreshold = this->getHistory()->average_tpt();
				if (DEBUG) std::cout << "SR " << this->getHistory()->success_ratio() << " low. Find rate exceeding ACTUAL_TPT " << throughputThreshold << "\n";
			}
			std::vector<int> newThroughputs = this->TheoreticalThroughputTables[std::make_tuple(std::get<0>(newColumnParameters), this->bandWidth, std::get<2>(newColumnParameters), this->agg)];
			for (uint i=0; i<newThroughputs.size(); ++i) {
				if (newThroughputs[i] != 0) {
					newIndex = i;
					//if (DEBUG) std::cout << newThroughputs[i] << "\n";
				}
				if (newThroughputs[i]*100 > throughputThreshold) {
					if (DEBUG) std::cout << "Found " << i << " " << newThroughputs[i] << "  > " << throughputThreshold << "\n";
					break;
				}
			}
			if (newIndex == INVALID_INDEX) if (DEBUG) std::cout << "\n\nWARNING: INVALID INDEX\n\n";
			return newIndex;
		}

		void RateScaling() {
			bool update_lq = false, done_search = false;
			int index = this->index;

			/* If we don't have enough data, keep gathering statistics */
			if (this->getHistory()->average_tpt() == INVALID_THROUGHPUT) {
				if (DEBUG) std::cout << "Test Window " << this->index << " : succ " << this->getHistory()->success_counter() << " total " << this->getHistory()->counter() << "\n";
				this->stay_in_table();
				return;
			}

			if (this->columnScaling) {
				/* If we are searching for a better column, check if it's working */
				if (DEBUG) std::cout << this->getHistory()->average_tpt() << "\n";
				if (this->getHistory()->average_tpt() > last_tpt) {
					if (DEBUG) std::cout << "SWITCHING TO NEW TABLE SR :" << this->getHistory()->success_ratio() << " cur-tpt " << this->getHistory()->average_tpt() << " old-tpt " << this->last_tpt << "\n";
				} else {
					if (DEBUG) std::cout << "GOING BACK TO THE OLD TABLE: SR: " << this->getHistory()->success_ratio() << " cur-tpt " << this->getHistory()->average_tpt() << " old-tpt " << this->last_tpt << "\n";
					this->mode = std::get<0>(this->oldColumnParameters);
					this->antennas = std::get<1>(this->oldColumnParameters);
					this->guardInterval = std::get<2>(this->oldColumnParameters);
					this->bandWidth = std::get<4>(this->oldColumnParameters);
					if (DEBUG) std::cout << "Old index " << index << "\n";
					index = std::get<3>(this->oldColumnParameters);

					if (this->mode == LEGACY) {
						this->type = LEGACY_G;
					} else {
						this->type = NONE;
					}

					update_lq = true;
				}
				this->columnScaling = false;
				done_search = true;
			} else {
				/* Else, we do MCS Scaling */
				std::tuple<int, int> adjacentIndexes = this->getAdjacentRatesIndexes();
				std::tuple<int, int> adjacentRates = {INVALID_THROUGHPUT, INVALID_THROUGHPUT};
				if (std::get<0>(adjacentIndexes) != INVALID_INDEX) {
					std::get<0>(adjacentRates) = this->getHistory(std::get<0>(adjacentIndexes))->average_tpt();
				}
				if (std::get<1>(adjacentIndexes) != INVALID_INDEX) {
					std::get<1>(adjacentRates) = this->getHistory(std::get<1>(adjacentIndexes))->average_tpt();
				}
				if (DEBUG) std::cout << "cur_tpt " << this->getHistory()->average_tpt() << " SR " << this->getHistory()->success_ratio() << " low " << std::get<0>(adjacentIndexes) << " high " << std::get<1>(adjacentIndexes) << " low_tpt " << std::get<0>(adjacentRates) << " high_tpt " << std::get<1>(adjacentRates) << "\n";
				rs_action scale_action = this->MCSScaling(adjacentIndexes, adjacentRates);
				switch(scale_action) {
					case RS_ACTION_DOWNSCALE:
						if(std::get<0>(adjacentIndexes) != INVALID_INDEX) {
							update_lq = true;
							index = std::get<0>(adjacentIndexes);
						} else {
							if (DEBUG) std::cout << "At the bottom rate. Can't decrease\n";
						}
						break;
					case RS_ACTION_UPSCALE:
						if(std::get<1>(adjacentIndexes) != INVALID_INDEX) {
							update_lq = true;
							index = std::get<1>(adjacentIndexes);
						} else {
							if (DEBUG) std::cout << "At the top rate. Can't increase\n";
						}
						break;
					case RS_ACTION_STAY:
						if (this->s == RS_STATE_STAY_IN_COLUMN) {
							/* Here, in the original Intel code, transmission power adaptation is done (rs_tpt_perform).
							 * This wasn't done in this code, so patches welcome !
							 */
						}
						break;
					default:
						break;
				}
			}

			if(update_lq) {
				this->index = index;
			}

			this->stay_in_table();
			if(!update_lq && !done_search && (this->s == RS_STATE_SEARCH_CYCLE_STARTED) && this->getHistory()->counter()) {
				if (DEBUG) std::cout << "Saving last tpt\n";
				this->last_tpt = this->getHistory()->average_tpt();
				if (DEBUG) std::cout << "Start Search: update_lq " << update_lq << " done_search " << done_search << " rs_state " << this->s << " win->counter " << this->getHistory()->counter() << "\n";
				auto findColumn = this->getNextColumn(this->visited_columns);
				if (std::get<0>(findColumn)) {
					if (DEBUG) std::cout << "Switch to column.\n";

					std::tuple<column_mode, std::set<antenna>, guard_interval> newColumnParameters = std::get<1>(findColumn);

					this->oldColumnParameters = std::make_tuple(this->mode, this->antennas, this->guardInterval, this->index, this->bandWidth);

					int nextIndex = this->getNextIndex(newColumnParameters);

					this->columnScaling = true;
					this->mode = std::get<0>(newColumnParameters);
					if (this->mode == LEGACY) {
						this->type = LEGACY_G;
					} else {
						this->type = NONE;
						this->bandWidth = this->maxWidth;
					}

					this->antennas = std::get<1>(newColumnParameters);
					this->guardInterval = std::get<2>(newColumnParameters);
					this->index = nextIndex;

					this->visited_columns.insert(std::make_tuple(this->mode, this->antennas, this->guardInterval));
					/* We start in a new column with a clean history */
					this->clearHistories();
				} else {
					if (DEBUG) std::cout << "No more columns to explore in search cycle. Go to RS_STATE_SEARCH_CYCLE_ENDED\n";
					this->s = RS_STATE_SEARCH_CYCLE_ENDED;
					// TODO: FIXME
					done_search = 1;
				}
			}

			if (done_search && this->s == RS_STATE_SEARCH_CYCLE_ENDED) {
				this->set_stay_in_table();
			}
		}

		std::tuple<ns3::WifiMode, int, int, int, int, int, bool, bool> getTxVector(bool ht, bool vht) {
			std::string rate  ;
			int index = this->index;
			//if (DEBUG) std::cout << "this->index = " << index << "\n";
			int nss = 1;
			if (this->mode == SISO) {
				if (index == 4) {
					index = 0;
				} else {
					index -=5;
				}
				if (!vht) {
					rate = "HtMcs" + std::to_string(index);
				} else {
					rate = "VhtMcs" + std::to_string(index);
				}
			} else if (this->mode == MIMO) {
				nss = 2;
				if (!vht) {
					if (index == 4) {
						index = 8;
					} else {
						index += 3;
					}
					rate = "HtMcs" + std::to_string(index);
				} else {
					if (index == 4) {
						index = 0;
					} else {
						index -= 5;
					}
					rate = "VhtMcs" + std::to_string(index);
				}
			} else {
				if (index <= 4) index = 4;
				switch(index) {
					case 0:
						rate = "DsssRate1Mbps";
						break;
					case 1:
						rate = "DsssRate2Mbps";
						break;
					case 2:
						rate = "DsssRate5_5Mbps";
						break;
					case 3:
						rate = "DsssRate11Mbps";
						break;
					case 4:
						rate = "OfdmRate6Mbps";
						break;
					case 5:
						rate = "OfdmRate9Mbps";
						break;
					case 6:
						rate = "OfdmRate12Mbps";
						break;
					case 7:
						rate = "OfdmRate18Mbps";
						break;
					case 8:
						rate = "OfdmRate24Mbps";
						break;
					case 9:
						rate = "OfdmRate36Mbps";
						break;
					case 10:
						rate = "OfdmRate48Mbps";
						break;
					case 11:
						rate = "OfdmRate54Mbps";
						break;
					default:
						if (DEBUG) std::cout << "Warning default case legacy rate\n";
						break;

				}
			}

			ns3::WifiMode mode(rate);
			return std::make_tuple(mode, this->guardInterval ? 800 : 400, this->antennas.size(), nss, 0, this->bandWidth, this->agg, false);
		}

		void tx(int success, int failed, bool ampdu) {
			// TODO: Check if Tx idle for too long

			if (ampdu && success == 0) { // We missed the block ack
				failed = 1;
			}

			for(int i=0; i<success; ++i) {
				this->getHistory()->tx(true);
			}

			for(int i=0; i<failed; ++i) {
				this->getHistory()->tx(false);
			}

			if (this->s == RS_STATE_STAY_IN_COLUMN) {
				this->total_success += success;
				this->total_failed += failed;
			}

			this->RateScaling();
		}
};


namespace ns3 {
	NS_LOG_COMPONENT_DEFINE("IntelWifiManager");
	NS_OBJECT_ENSURE_REGISTERED(IntelWifiManager);

	struct IntelWifiRemoteStation : public WifiRemoteStation
	{
		State state;
	};


	TypeId
		IntelWifiManager::GetTypeId (void) {
			static TypeId tid = TypeId ("ns3::IntelWifiManager")
				.SetParent<WifiRemoteStationManager> ()
				.SetGroupName ("Wifi")
				.AddConstructor<IntelWifiManager> ()
				.AddAttribute ("DataMode", "The transmission mode to use for every data packet transmission",
						StringValue ("OfdmRate6Mbps"),
						MakeWifiModeAccessor (&IntelWifiManager::m_dataMode),
						MakeWifiModeChecker ())
				.AddAttribute ("ControlMode", "The transmission mode to use for every RTS packet transmission.",
						StringValue ("OfdmRate6Mbps"),
						MakeWifiModeAccessor (&IntelWifiManager::m_ctlMode),
						MakeWifiModeChecker ())
				;
			return tid;
		}


	WifiModeList IntelWifiManager::GetVhtDeviceMcsList (void) const
	{
		WifiModeList vhtMcsList;
		Ptr<WifiPhy> phy = GetPhy ();
		for (uint8_t i = 0; i < phy->GetNMcs (); i++)
		{
			WifiMode mode = phy->GetMcs (i);
			if (mode.GetModulationClass () == WIFI_MOD_CLASS_VHT)
			{
				vhtMcsList.push_back (mode);
			}
		}
		return vhtMcsList;
	}

	WifiModeList IntelWifiManager::GetHtDeviceMcsList (void) const
	{
		WifiModeList htMcsList;
		Ptr<WifiPhy> phy = GetPhy ();
		for (uint8_t i = 0; i < phy->GetNMcs (); i++)
		{
			WifiMode mode = phy->GetMcs (i);
			if (mode.GetModulationClass () == WIFI_MOD_CLASS_HT)
			{
				htMcsList.push_back (mode);
			}
		}
		return htMcsList;
	}

	bool IntelWifiManager::IsValidMcs (Ptr<WifiPhy> phy, uint8_t streams, uint16_t chWidth, WifiMode mode) {
		NS_LOG_FUNCTION (this << phy << +streams << chWidth << mode);
		if (DEBUG) std::cout << "IsValidMcs: " << phy << +streams << chWidth << mode << "\n";
		WifiTxVector txvector;
		txvector.SetNss (streams);
		txvector.SetChannelWidth (chWidth);
		txvector.SetMode (mode);
		return txvector.IsValid ();
	}


	void IntelWifiManager::DoInitialize () {
		if (DEBUG) std::cout << "DoInitialize\n";
		NS_LOG_FUNCTION (this);

		if (GetHtSupported ()) {
			if (DEBUG) std::cout << "Ht supported!\n";
			WifiModeList htMcsList = GetHtDeviceMcsList ();
		}

		if (GetVhtSupported ()) {
			if (DEBUG) std::cout << "Vht supported!\n";
			WifiModeList vhtMcsList = GetVhtDeviceMcsList ();
		}

		if (!(GetVhtSupported() || GetHtSupported())) {
			if (DEBUG) std::cout << "Device does not support HT or VHT !\n";
		}

	}

	IntelWifiManager::IntelWifiManager () {
		NS_LOG_FUNCTION (this);
	}

	IntelWifiManager::~IntelWifiManager () {
		NS_LOG_FUNCTION (this);
	}

	WifiRemoteStation * IntelWifiManager::DoCreateStation (void) const {
		NS_LOG_FUNCTION (this);
		if (DEBUG) std::cout << "Creating new station\n";
		IntelWifiRemoteStation *station = new IntelWifiRemoteStation ();
		station->state = State(GetPhy()->GetChannelWidth());
		return station;
	}

	void IntelWifiManager::DoReportRxOk (WifiRemoteStation *station, double rxSnr, WifiMode txMode) {
		NS_LOG_FUNCTION (this << station << rxSnr << txMode);
	}

	void IntelWifiManager::DoReportRtsFailed (WifiRemoteStation *station) {
		NS_LOG_FUNCTION (this << station);
	}

	void IntelWifiManager::DoReportDataFailed (WifiRemoteStation *st) {
		NS_LOG_FUNCTION (this << st);
		IntelWifiRemoteStation *station = (IntelWifiRemoteStation *) st;
		station->state.tx(0, 1, false);
	}

	void IntelWifiManager::DoReportRtsOk (WifiRemoteStation *st, double ctsSnr, WifiMode ctsMode, double rtsSnr) {
		NS_LOG_FUNCTION (this << st << ctsSnr << ctsMode << rtsSnr);
	}

	void IntelWifiManager::DoReportDataOk (WifiRemoteStation *st, double ackSnr, WifiMode ackMode, double dataSnr, uint16_t dataChannelWidth, uint8_t dataNss) {
		NS_LOG_FUNCTION (this << st);
		IntelWifiRemoteStation *station = (IntelWifiRemoteStation *) st;
		station->state.tx(1, 0, false);
		NS_LOG_FUNCTION (this << st << ackSnr << ackMode << dataSnr);
	}

	void IntelWifiManager::DoReportAmpduTxStatus (WifiRemoteStation *st, uint8_t nSuccessfulMpdus, uint8_t nFailedMpdus, double rxSnr, double dataSnr, uint16_t dataChannelWidth, uint8_t dataNss) {
		IntelWifiRemoteStation *station = (IntelWifiRemoteStation *) st;
		station->state.tx(nSuccessfulMpdus, nFailedMpdus, true);
	}

	void IntelWifiManager::DoReportFinalRtsFailed (WifiRemoteStation *station) {
		NS_LOG_FUNCTION (this << station);
	}

	void IntelWifiManager::DoReportFinalDataFailed (WifiRemoteStation *station) {
		NS_LOG_FUNCTION (this << station);
	}

	WifiTxVector IntelWifiManager::DoGetDataTxVector (WifiRemoteStation *st) {
		NS_LOG_FUNCTION (this << st);
		IntelWifiRemoteStation *station = (IntelWifiRemoteStation *) st;
		std::tuple<ns3::WifiMode, int, int, int, int, int, bool, bool> parameters = station->state.getTxVector(GetHtSupported(), GetVhtSupported());
		return WifiTxVector (std::get<0>(parameters), GetDefaultTxPowerLevel (), GetPreambleForTransmission (std::get<0>(parameters).GetModulationClass (), (std::get<1>(parameters)==400), false), std::get<1>(parameters), std::get<2>(parameters), std::get<3>(parameters), std::get<4>(parameters), std::get<5>(parameters), std::get<6>(parameters), std::get<7>(parameters));
	}

	WifiTxVector IntelWifiManager::DoGetRtsTxVector (WifiRemoteStation *st) {
		NS_LOG_FUNCTION (this << st);
		if (DEBUG) std::cout << "Warning: RTS/CTS not yet fully supported.\n";
		return WifiTxVector (m_ctlMode, GetDefaultTxPowerLevel (), GetPreambleForTransmission (m_ctlMode.GetModulationClass (), GetShortPreambleEnabled (), UseGreenfieldForDestination (GetAddress (st))), ConvertGuardIntervalToNanoSeconds (m_ctlMode, GetShortGuardIntervalSupported (st), NanoSeconds (GetGuardInterval (st))), 1, 1, 0, GetChannelWidthForTransmission (m_ctlMode, GetChannelWidth (st)), GetAggregation (st), false);
	}

	bool IntelWifiManager::IsLowLatency (void) const {
		return true;
	}
}
