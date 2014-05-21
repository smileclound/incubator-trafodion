#include "mxomsg.h"
#ifndef DWORD
#define DWORD unsigned int
#endif

struct msg
{
	char sever[10];
	short id;
	char text[250];
} msgarray[] = {
	"ERROR",  IDS_01_002, "01002 01001 DISCONNECT ERROR. TRANSACTION ROLLED BACK.",//  1
	"ERROR",  IDS_01_004, "01004 01004 DATA TRUNCATED.",//  2
	"ERROR",  IDS_01_006, "01006 01006 PRIVILEGE NOT REVOKED.",//  3
	"ERROR",  IDS_07_001, "07001 01008 WRONG NUMBER OF PARAMETERS.",//  4
	"ERROR",  IDS_07_003, "07003 01010 DYNAMIC SQL ERROR.  CURSOR SPECIFICATION CANNOT BE EXECUTED.",//  5
	"ERROR",  IDS_07_005, "07005 01012 DYNAMIC SQL ERROR.  PREPARED STATEMENT IS NOT A CURSOR SPECIFICATION.",//  6
	"ERROR",  IDS_07_006, "07006 01014 RESTRICTED DATA TYPE ATTRIBUTE VIOLATION.",//  7
	"ERROR",  IDS_07_008, "07008 01016 DYNAMIC SQL ERROR.  WRONG NUMBER OF BOUND COLUMNS.",//  8
	"ERROR",  IDS_08_001, "08001 01018 UNABLE TO CONNECT TO DATA SOURCE.",//  9
	"ERROR",  IDS_08_001_01, "08001 01018 UNABLE TO CONNECT TO DATA SOURCE. THE ASSOCIATION SERVICE ENTRY IS NOT FOUND OR EMPTY.",//  10
	"ERROR",  IDS_08_001_02, "08001 01018 UNABLE TO CONNECT TO DATA SOURCE. THE DRIVER ENTRY IS NOT FOUND OR EMPTY.",//  11
	"ERROR",  IDS_08_002, "08002 01020 CONNECTION IN USE.",//  12
	"ERROR",  IDS_08_003, "08003 01022 CONNECTION NOT OPEN.",//  13
	"ERROR",  IDS_08_004, "08004 01024 DATA SOURCE REJECTED ESTABLISHMENT OF CONNECTION FOR IMPLEMENTATION DEFINED REASONS.",//  14
	"ERROR",  IDS_08_005, "08005 01026 COMMUNICATION FAILURE.",//  15
	"ERROR",  IDS_08_006, "08006 01028 TRANSACTION ROLLEDBACK.",//  16
	"ERROR",  IDS_08_007, "08007 01030 CONNECTION FAILURE DURING TRANSACTION.",//  17
	"ERROR",  IDS_08_S01, "08S01 01032 COMMUNICATION LINK FAILURE. THE SERVER TIMED OUT OR DISAPPEARED.",//  18
	"ERROR",  IDS_21_001, "21001 01034 CARDINALITY VIOLATION; INSERT VALUE LIST DOES NOT MATCH COLUMN LIST.",//  19
	"ERROR",  IDS_21_002, "21002 01036 CARDINALITY VIOLATION; PARAMETER VALUE LIST DOES NOT MATCH COLUMN LIST.",//  20
	"ERROR",  IDS_21_S01, "21S01 01038 CARDINALITY VIOLATION; INSERTION VALUE LIST DOES NOT MATCH COLUMN LIST.",//  21
	"ERROR",  IDS_21_S02, "21S02 01040 CARDINALITY VIOLATION; PARAMETER LIST DOES NOT MATCH COLUMN LIST.",//  22
	"ERROR",  IDS_22_001, "22001 01042 STRING DATA RIGHT TRUNCATION.",//  23
	"ERROR",  IDS_22_003, "22003 01044 NUMERIC VALUE OUT OF RANGE.",//  24
	"ERROR",  IDS_22_005, "22005 01046 ERROR IN ASSIGNMENT.",//  25
	"ERROR",  IDS_22_005_01, "22005 PRECISION OR SCALE OUT OF RANGE.",//  26
	"ERROR",  IDS_22_012, "22012 01050 DIVISION BY ZERO.",//  27
	"ERROR",  IDS_23_000, "23000 01052 INTEGRITY CONSTRAINT VIOLATION.",//  28
	"ERROR",  IDS_24_000, "24000 01054 INVALID CURSOR STATE.",//  29
	"ERROR",  IDS_25_000, "25000 01056 INVALID TRANSACTION STATE.",//  30
	"ERROR",  IDS_26_000, "26000 01058 INVALID SQL STATEMENT IDENTIFIER.",//  31
	"ERROR",  IDS_28_000, "28000 01060 INVALID AUTHORIZATION SPECIFICATION.",//  32
	"ERROR",  IDS_28_000_00, "28000 01062 INVALID AUTHORIZATION SPECIFICATION; ACCESS TO SELECTED DATABASE HAS BEEN DENIED.",//  33
	"ERROR",  IDS_34_000, "34000 01064 INVALID CURSOR NAME.",//  34
	"ERROR",  IDS_37_000, "37000 01066 SYNTAX ERROR IN SQL DYNAMIC STATEMENT.",//  35
	"ERROR",  IDS_3C_000, "3C000 01068 DUPLICATE CURSOR NAME.",//  36
	"ERROR",  IDS_40_001, "40001 01070 ATTEMPT TO INITIATE NEW SQL SERVER OPERATION WITH DATA PENDING.",//  37
	"ERROR",  IDS_42_000, "42000 01072 SYNTAX ERROR OR ACCESS RULE VIOLATION.",//  38
	"ERROR",  IDS_70_100, "70100 01074 OPERATION ABORTED (SERVER DID NOT PROCESS CANCEL REQUEST).",//  39
	"ERROR",  IDS_S0_001, "S0001 01076 INVALID TABLE NAME; BASE TABLE OR VIEW ALREADY EXISTS.",//  40
	"ERROR",  IDS_S0_002, "S0002 01078 INVALID TABLE NAME; TABLE OR VIEW NOT FOUND.",//  41
	"ERROR",  IDS_S0_011, "S0011 01080 INVALID INDEX NAME; INDEX ALREADY EXISTS.",//  42
	"ERROR",  IDS_S0_012, "S0012 01082 INVALID INDEX NAME; INDEX NOT FOUND.",//  43
	"ERROR",  IDS_S0_021, "S0021 01084 INVALID COLUMN NAME; COLUMN ALREADY EXISTS.",//  44
	"ERROR",  IDS_S0_022, "S0022 01086 INVALID COLUMN NAME; COLUMN NOT FOUND.",//  45
	"ERROR",  IDS_S1_000, "S1000 01088 GENERAL ERROR.",//  46
	"ERROR",  IDS_S1_000_00, "S1000 01090 GENERAL ERROR: ONGOING TRANSACTION HAS BEEN COMMITED.",//  47
	"ERROR",  IDS_S1_000_01, "S1000 01092 THE STORED PROCEDURE REQUIRED TO COMPLETE THIS OPERATION COULD NOT BE FOUND ON THE SERVER (THEY WERE SUPPLIED WITH THE ODBC SETUP DISK FOR THE SQL SERVER DRIVER). CONTACT YOUR SERVICE PROVIDER.",//  48
	"ERROR",  IDS_S1_000_02, "S1000 01094 UNKNOWN TOKEN RECEIVED FROM SQL SERVER.",//  49
	"ERROR",  IDS_S1_000_03, "S1000 01096 THE ODBC CATALOG STORED PROCEDURES INSTALLED ON SERVER 0S ARE VERSION 0S; VERSION 002D.002D.04.4D IS REQUIRED TO ENSURE PROPER OPERATION.  PLEASE CONTACT YOUR SYSTEM ADMINISTRATOR.",//  50
	"ERROR",  IDS_S1_000_04, "S1000 01098 UNABLE TO LOAD COMMUNICATION MODULE.  DRIVER HAS NOT BEEN CORRECTLY INSTALLED.",//  51
	"ERROR",  IDS_S1_000_05, "S1000 01100 COMMUNICATION MODULE IS NOT VALID.  DRIVER HAS NOT BEEN CORRECTLY INSTALLED.",//  52
	"ERROR",  IDS_S1_000_06, "S1000 01102 DATA TYPE MISMATCH.",//  53
	"ERROR",  IDS_S1_001, "S1001 01104 MEMORY ALLOCATION ERROR.",//  54
	"ERROR",  IDS_S1_002, "S1002 01106 INVALID COLUMN NUMBER.",//  55
	"ERROR",  IDS_S1_003, "S1003 01108 PROGRAM TYPE OUT OF RANGE.",//  56
	"ERROR",  IDS_S1_004, "S1004 01110 SQL DATA TYPE OUT OF RANGE.",//  57
	"ERROR",  IDS_S1_005, "S1005 01112 PARAMETER NUMBER OUT OF RANGE.",//  58
	"ERROR",  IDS_S1_006, "S1006 01114 INVALID CONVERSION SPECIFIED.",//  59
	"ERROR",  IDS_S1_007, "S1007 01116 ROW COUNT NOT AVAILABLE FROM THE DATA SOURCE.",//  60
	"ERROR",  IDS_S1_008, "S1008 01118 OPERATION CANCELLED.",//  61
	"ERROR",  IDS_S1_009, "S1009 01120 INVALID ARGUMENT VALUE.",//  62
	"ERROR",  IDS_S1_010, "S1010 01122 FUNCTION SEQUENCE ERROR.",//  63
	"ERROR",  IDS_S1_012, "S1012 01124 INVALID TRANSACTION OPERATOR CODE SPECIFIED.",//  64
	"ERROR",  IDS_HY_015, "S1015 01130 NO CURSOR NAME AVAILABLE.",//  65
	"ERROR",  IDS_S1_090, "S1090 01132 INVALID STRING OR BUFFER LENGTH.",//  66
	"ERROR",  IDS_S1_091, "S1091 01134 DESCRIPTOR TYPE OUT OF RANGE.",//  67
	"ERROR",  IDS_S1_092, "S1092 01136 OPTION TYPE OUT OF RANGE.",//  68
	"ERROR",  IDS_S1_093, "S1093 01138 INVALID PARAMETER NUMBER OR MISSING PARAMETER.",//  69
	"ERROR",  IDS_S1_094, "S1094 01140 INVALID SCALE VALUE.",//  70
	"ERROR",  IDS_S1_095, "S1095 01142 FUNCTION TYPE OUT OF RANGE.",//  71
	"ERROR",  IDS_S1_096, "S1096 01144 INFORMATION TYPE OUT OF RANGE.",//  72
	"ERROR",  IDS_S1_097, "S1097 01146 COLUMN TYPE OUT OF RANGE.",//  73
	"ERROR",  IDS_S1_098, "S1098 01148 SCOPE TYPE OUT OF RANGE.",//  74
	"ERROR",  IDS_S1_099, "S1099 ZZZZZ NULLABLE TYPE OUT OF RANGE.",//  75
	"ERROR",  IDS_S1_100, "S1100 ZZZZZ UNIQUENESS OPTION TYPE OUT OF RANGE.",//  76
	"ERROR",  IDS_S1_101, "S1101 ZZZZZ ACCURACY OPTION TYPE OUT OF RANGE.",//  77
	"ERROR",  IDS_S1_102, "S1102 ZZZZZ TABLE TYPE OUT OF RANGE.",//  78
	"ERROR",  IDS_S1_103, "S1103 ZZZZZ DIRECTION OPTION OUT OF RANGE.",//  79
	"ERROR",  IDS_S1_106, "S1106 ZZZZZ FETCH TYPE OUT OF RANGE.",//  80
	"ERROR",  IDS_S1_107, "S1107 ZZZZZ ROW VALUE OUT OF RANGE.",//  81
	"ERROR",  IDS_S1_108, "S1108 ZZZZZ CONCURRENCY OPTION OUT OF RANGE.",//  82
	"ERROR",  IDS_S1_109, "S1109 ZZZZZ INVALID CURSOR POSITION; NO KEYSET DEFINED.",//  83
	"ERROR",  IDS_S1_C00, "S1C00 ZZZZZ DRIVER NOT CAPABLE.",//  84
	"ERROR",  IDS_S1_LD0, "S1LD0 ZZZZZ NO LONG DATA VALUES PENDING.",//  85
	"ERROR",  IDS_S1_T00, "S1T00 ZZZZZ TIMEOUT EXPIRED.",//  86
	"ERROR",  IDS_IM_001, "IM001 ZZZZZ DRIVER DOES NOT SUPPORT THIS FUNCTION.",//  87
	"ERROR",  IDS_01_000, "01000 ZZZZZ GENERAL WARNING.",//  88
	"ERROR",  IDS_01_S02, "01S02 ZZZZZ OPTION VALUE CHANGED.",//  89
	"ERROR",  IDS_22_008, "22008 ZZZZZ DATETIME FIELD OVERFLOW.",//  90
	"ERROR",  IDS_S1_105, "S1105 ZZZZZ INVALID PARAMETER TYPE OR PARAMETER TYPE NOT SUPPORTED.",//  91
	"ERROR",  IDS_IM_009, "IM009 ZZZZZ UNABLE TO LOAD DLL.",//  92
	"ERROR",  IDS_186_DSTODRV_TRUNC, "HY721 ZZZZZ TRANSLATIONDLL TRUNCATION: DATASOURCETODRIVER [01!S!].",//  93
	"ERROR",  IDS_188_DRVTODS_TRUNC, "HY721 ZZZZZ TRANSLATIONDLL TRUNCATION: DRIVERTODATASOURCE [01!S!].",//  94
	"ERROR",  IDS_190_DSTODRV_ERROR, "HY722 ZZZZZ TRANSLATIONDLL ERROR: DATASOURCETODRIVER [01!S!].",//  95
	"ERROR",  IDS_193_DRVTODS_ERROR, "HY723 ZZZZZ TRANSLATIONDLL ERROR: DRIVERTODATASOURCE [01!S!].",//  96
	"ERROR",  IDS_08_S02, "08S02 01033 TRANSPORT LAYER ERROR.",//  97
	"ERROR",  IDS_07_009_02, "07009 ZZZZZ The value specified for the argument ColumnNumber was greater than the number of columns in the result set.",//98
	"ERROR",  IDS_22_003_02, "22003 ZZZZZ A negative value cannot be converted to an unsigned numeric datatype.",//99
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"ERROR",  IDS_NO_SRVR_AVAILABLE, "08001 ZZZZZ NO MORE ODBC SERVERS AVAILABLE TO CONNECT.",//  150
	"ERROR",  IDS_ASSOC_SRVR_NOT_AVAILABLE, "08001 ZZZZZ ODBC SERVICES NOT YET AVAILABLE: [01!S!].",//  151
	"ERROR",  IDS_DS_NOT_AVAILABLE, "08001 ZZZZZ DATASOURCE NOT YET AVAILABLE OR NOT FOUND: [01!S!].",//  152
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"ERROR",  IDS_PROGRAM_ERROR, "S1000 ZZZZZ UNEXPECTED PROGRAMMING EXCEPTION HAS BEEN FOUND: [01!S!].  CHECK THE SERVER EVENT LOG ON NODE [02!S!] FOR DETAILS.",//  200
	"ERROR",  IDS_PORT_NOT_AVAILABLE, "08001 ZZZZZ NO MORE PORTS AVAILABLE TO START ODBC SERVERS.",//  201
	"ERROR",  IDS_RETRY_ATTEMPT_EXCEEDED, "08001 ZZZZZ RETRY ATTEMPTS TO CONNECT TO THE DATASOURCE FAILED, MAY BE ODBC SERVER NOT ABLE TO REGISTER TO THE ODBC SERVICE PROCESS.",//  202
	"WARNING",  IDS_01_000_01, "01000 ZZZZZ GENERAL WARNING. CONNECTED TO THE DEFAULT DATA SOURCE: [01!S!].",//  203
	"WARNING",  IDS_S1_000_07, "S1000 ZZZZZ GENERAL ERROR. FAILED SINCE RESOURCE GOVERNING POLICY IS HIT: [01!S!].",//  204
	"WARNING",  IDS_08_004_01, "A DIFFERENT CLIENT NOW.",//  205
	"WARNING",  IDS_RG_LOG_INFO, "01000 ZZZZZ QUERY ESTIMATED COST, [01!S!], EXCEEDS RESOURCE POLICY [02!S!].  STATEMENT WRITTEN TO LOG.",//  206
	"",0,"",
	"",0,"",
	"",0,"",
	"WARNING",  IDS_EXCEPTION_MSG, "S1000 ZZZZZ GENERAL ERROR. RUNTIME EXCEPTION [01!S!] HAS BEEN DETECTED IN FUNCTION [02!S!].",//  210
	"WARNING",  IDS_NO_FUNCTION, "S1000 ZZZZZ GENERAL ERROR. THE CALLED FUNCTION [01!S!] IS NOT IMPLEMENTED.  PLEASE CONTACT YOUR SERVICE PROVIDER.",//  211
	"WARNING",  IDS_SQL_ERROR, "S1000 ZZZZZ SQL ERROR:",//  212
	"WARNING",  IDS_SQL_WARNING, "01000 ZZZZZ SQL WARNING:",//  213
	"WARNING",  IDS_UNABLE_TO_CANCEL_BY_SQL, "S1000 ZZZZZ UNABLE TO CANCEL THE STATEMENT BECAUSE OF A SQL ERROR.",//  214
	"WARNING",  IDS_THREAD_ERROR, "S1000 ZZZZZ AN INTERNAL THREAD ERROR HAS BEEN DETECTED.  ERROR MESSAGE: [01!S!].",//  215
	"WARNING",  IDS_UNABLE_TO_GETPARAM, "S1000 ZZZZZ UNABLE TO GET PARAMETER INFORMATION EITHER BECAUSE OF A NULL POINTER OR AN INCOMPLETE PARAMETER LIST.",//  216
	"WARNING",  IDS_TRANSACTION_ERROR, "S1000 ZZZZZ A TIP TRANSACTION ERROR [01!S!] HAS BEEN DETECTED.  CHECK THE SERVER EVENT LOG ON NODE [02!S!] FOR TRANSACTION ERROR DETAILS.",//  217
	"",0,"",
	"",0,"",
	"WARNING",  IDS_NT_ERROR, "S1000 ZZZZZ GENERAL ERROR. THE FUNCTION [01!S!] HAS DETECTED AN NT ERROR: [02!S!].",//  220
	"WARNING",  IDS_UNABLE_TO_LOAD_LIBRARY, "S1000 ZZZZZ UNABLE TO LOAD THE HP ODBC DRIVER BECAUSE OF AN NT ERROR: [01!S!].",//  221
	"WARNING",  IDS_UNABLE_TO_GET_VERSION, "S1000 ZZZZZ UNABLE TO GET THE OS VERSION NUMBERS BECAUSE OF AN NT ERROR: [01!S!].",//  222
	"WARNING",  IDS_UNABLE_TO_GET_USER, "S1000 ZZZZZ UNABLE TO GET THE USER NAME BECAUSE OF AN NT ERROR: [01!S!].",//  223
	"WARNING",  IDS_UNABLE_TO_GET_USER_DESC, "S1000 ZZZZZ UNABLE TO GET THE USER DESCRIPTION BECAUSE OF AN NT ERROR: [01!S!].",//  224
	"WARNING",  IDS_UNABLE_TO_LOGON, "S1000 ZZZZZ UNABLE TO AUTHENTICATE THE USER BECAUSE OF AN NT ERROR: [01!S!].",//  225
	"WARNING",  IDS_UNABLE_TO_GETODBCINI, "S1000 ZZZZZ UNABLE TO RETRIEVE THE ODBC.INI STRING FOR KEY [01!S!] BECAUSE OF AN NT ERROR: [02!S!].",//  226
	"WARNING",  IDS_UNABLE_TO_GETMODULENAME, "S1000 ZZZZZ UNABLE TO RETRIEVE THE DRIVER'S MODULE NAME BECAUSE OF AN NT ERROR: [01!S!].",//  227
	"WARNING",  IDS_UNABLE_TO_GETFILEVERSION, "S1000 ZZZZZ UNABLE TO RETRIEVE THE DRIVER'S FILE VERSION BECAUSE OF AN NT ERROR: [01!S!].",//  228
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"WARNING",  IDS_KRYPTON_UNABLE_TO_INITIALIZE, "S1000 ZZZZZ UNABLE TO INITIALIZE THE COMMUNICATION LAYER BECAUSE OF A NETWORK ERROR: [01!S!].",//  300
	"WARNING",  IDS_KRYPTON_UNABLE_TO_TERMINATE, "S1000 ZZZZZ UNABLE TO TERMINATE THE DIALOGUE WITH THE MXCS SERVER BECAUSE OF A NETWORK ERROR: [01!S!].",//  301
	"WARNING",  IDS_KRYPTON_ODBCSRVR_INTERFACE_FAILED, "S1000 ZZZZZ UNABLE TO REGISTER THE MXCS SERVER INTERFACE BECAUSE OF A NETWORK ERROR: [01!S!].",//  302
	"WARNING",  IDS_KRYPTON_ASSRVR_INTERFACE_FAILED, "S1000 ZZZZZ UNABLE TO REGISTER THE ASSOCIATION SERVER INTERFACE BECAUSE OF A NETWORK ERROR: [01!S!].",//  303
	"WARNING",  IDS_KRYPTON_UNABLE_PROXY_CREATE, "S1000 ZZZZZ UNABLE TO CREATE THE PROXY FOR THE MXCS SERVER BECAUSE OF A NETWORK ERROR: [01!S!].",//  304
	"WARNING",  IDS_KRYPTON_UNABLE_PROXY_DESTROY, "S1000 ZZZZZ UNABLE TO DESTROY THE PROXY FOR THE MXCS SERVER BECAUSE OF A NETWORK ERROR: [01!S!].",//  305
	"",0,"",
	"",0,"",
	"",0,"",
	"",0,"",
	"WARNING",  IDS_KRYPTON_ERROR, "S1000 ZZZZZ UNABLE TO PERFORM FUNCTION [01!S!] BECAUSE OF A NETWORK ERROR: [02!S!].",//  310
	"WARNING",  IDS_KRYPTON_SRVR_GONE, "08S01 ZZZZZ COMMUNICATION LINK FAILURE.  UNABLE TO PERFORM FUNCTION [01!S!] BECAUSE THE MXCS SERVER DISAPPEARED.",//  311
	"WARNING",  IDS_KRYPTON_NO_SRVR, "S1000 ZZZZZ UNABLE TO PERFORM FUNCTION [01!S!] BECAUSE THE MXCS SERVER DOES NOT EXIST.",//  312
	"WARNING",  IDS_DS_NOT_FOUND, "08001 ZZZZZ UNABLE TO CONNECT TO DATA SOURCE. THE DATA SOURCE ENTRY IS NOT FOUND.",//  313
	"WARNING",  IDS_HY_000, "HY000 01088 GENERAL ERROR.",//  314
	"WARNING",  IDS_HY_001, "HY001 01089 MEMORY ALLOCATION ERROR.",//  315
	"WARNING",  IDS_HY_090, "HY090 01090 INVALID STRING OR BUFFER LENGTH.",//  316
	"WARNING",  IDS_IM_010, "IM010 01091 DATA SOURCE NAME TOO LONG.",//  317
	"WARNING",  IDS_CEE_THREAD_NOT_AVAILABLE, "HY000 01092 GENERAL ERROR - UNRECOVERABLE ERROR - THE NETWORK COMPONENT THREAD HAS BEEN TERMINATED EARLIER.",//  318
	"WARNING",  IDS_HY_092, "HY092 01093 INVALID ATTRIBUTE/OPTION IDENTIFIER.",//  319
	"WARNING",  IDS_HY_C00, "HYC00 01093 OPTIONAL FEATURE NOT IMPLEMENTED.",//  320
	"WARNING",  IDS_HY_091, "HY091 01094 INVALID DESCRIPTOR FIELD IDENTIFIER.",//  321
	"WARNING",  IDS_HY_016, "HY016 01095 CANNOT MODIFY AN IMPLEMENTATION ROW DESCRIPTOR.",//  322
	"WARNING",  IDS_07_009, "07009 01096 INVALID DESCRIPTOR INDEX. RECNUMBER IS 0 (BOOKMARK IS NOT YET SUPPORTED).",//  323
	"WARNING",  IDS_HY_021, "HY021 01096 INCONSISTENT DESCRIPTOR INFORMATION.",//  324
	"WARNING",  IDS_HY_003, "HY003 01097 INVALID APPLICATION BUFFER TYPE.",//  325
	"WARNING",  IDS_HY_013, "HY013 01098 MEMORY MANAGEMENT ERROR.",//  326
	"WARNING",  IDS_01_S00, "01S00 01099 INVALID CONNECTION STRING ATTRIBUTE.",//  327
	"WARNING",  IDS_IM_007, "IM007 01100 NO DATA SOURCE OR DRIVER SPECIFIED; DIALOG PROHIBITED.",//  328
	"WARNING",  IDS_IM_008, "IM008 01101 DIALOG FAILED.",//  329
	"WARNING",  IDS_HY_105, "HY105 01102 INVALID PARAMETER TYPE.",//  330
	"ERROR",    IDS_HY_018, "HY018 01103 SERVER DECLINED CANCEL REQUEST.",//  331
	"WARNING",  IDS_07_002, "07002 01104 COUNT FIELD INCORRECT.",//  332
	"WARNING",  IDS_07_S01, "07S01 01105 INVALID USE OF DEFAULT PARAMETER SQL_DEFAULT_PARAMETER NOT SUPPORTED.",//  333
	"WARNING",  IDS_HY_010, "HY010 01106 FUNCTION SEQUENCE ERROR.",//  334
	"WARNING",  IDS_HY_020, "HY020 01107 ATTEMPT TO CONCATENATE A NULL VALUE.",//  335
	"WARNING",  IDS_HY_019, "HY019 01108 NON-CHARACTER AND NON-BINARY DATA SENT IN PIECES.",//  336
	"WARNING",  IDS_22_002, "22002 01109 INDICATOR VARIABLE REQUIRED BUT NOT SUPPLIED.",//  337
	"WARNING",  IDS_01_S07, "01S07 01110 FRACTIONAL TRUNCATION.",//  338
	"WARNING",  IDS_HY_107, "HY107 01111 ROW-VALUE OUT OF RANGE.",//  339
	"WARNING",  IDS_22_018, "22018 01112 INVALID CHARACTER VALUE FOR CAST SPECIFICATION.",//  340
	"WARNING",  IDS_HY_024, "HY024 01113 INVALID ATTRIBUTE VALUE.",//  341
	"WARNING",  IDS_HY_110, "HY110 01114 INVALID DRIVER COMPLETION.",//  342
	"WARNING",  IDS_IM_002, "IM002 01115 DATA SOURCE NOT FOUND OR NO DEFAULT DRIVER SPECIFICATION.",//  343
	"WARNING",  IDS_HY_109, "HY009 01116 INVALID CURSOR POSITION.",//  344
	"WARNING",  IDS_HY_007, "HY007 01117 ASSOCIATED STATEMENT IS NOT PREPARED.",//  345
	"WARNING",  IDS_HY_096, "HY096 01118 INFORMATION TYPE OUT OF RANGE.",//  346
	"WARNING",  IDS_S1_000_08, "01000 ZZZZZ GENERAL WARNING. RESOURCE GOVERNING POLICY IS HIT: [01!S!].",//  347
	"WARNING",  IDS_HY_004, "HY004 01119 INVALID SQL DATA TYPE.",//  348
	"WARNING",  IDS_22_015, "22015 ZZZZZ INTERVAL FIELD OVERFLOW.",//  349
};
