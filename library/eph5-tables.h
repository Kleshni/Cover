#ifndef EPH5_TABLES_H
	#define EPH5_TABLES_H

	#include <stddef.h>

	#define EPH5_MAGIC_TABLE_2_LENGTH 55
	#define EPH5_MAGIC_TABLE_3_LENGTH 363
	#define EPH5_MAGIC_TABLE_4_LENGTH 335
	#define EPH5_MAGIC_TABLE_5_LENGTH 252
	#define EPH5_MAGIC_TABLE_6_LENGTH 386
	#define EPH5_MAGIC_TABLE_7_LENGTH 507

	extern const double Eph5_magic_table_2[EPH5_MAGIC_TABLE_2_LENGTH];
	extern const double Eph5_magic_table_3[EPH5_MAGIC_TABLE_3_LENGTH];
	extern const double Eph5_magic_table_4[EPH5_MAGIC_TABLE_4_LENGTH];
	extern const double Eph5_magic_table_5[EPH5_MAGIC_TABLE_5_LENGTH];
	extern const double Eph5_magic_table_6[EPH5_MAGIC_TABLE_6_LENGTH];
	extern const double Eph5_magic_table_7[EPH5_MAGIC_TABLE_7_LENGTH];

	extern const size_t Eph5_magic_tables_lengths[6];
	extern const double * const Eph5_magic_tables[6];
#endif
