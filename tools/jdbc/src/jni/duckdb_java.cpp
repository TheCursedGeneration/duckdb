#include "org_duckdb_DuckDBNative.h"
#include "duckdb.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/client_data.hpp"
#include "duckdb/catalog/catalog_search_path.hpp"
#include "duckdb/main/appender.hpp"
#include "duckdb/common/operator/cast_operators.hpp"
#include "duckdb/main/db_instance_cache.hpp"
#include "duckdb/common/arrow/result_arrow_wrapper.hpp"
#include "duckdb/function/table/arrow.hpp"
#include "duckdb/main/database_manager.hpp"
#include "duckdb/parser/parsed_data/create_type_info.hpp"

using namespace duckdb;
using namespace std;

static jint JNI_VERSION = JNI_VERSION_1_6;

// Static global vars of cached Java classes, methods and fields
static jclass J_Charset;
static jmethodID J_Charset_decode;
static jobject J_Charset_UTF8;

static jclass J_CharBuffer;
static jmethodID J_CharBuffer_toString;

static jmethodID J_String_getBytes;

static jclass J_SQLException;

static jclass J_Bool;
static jclass J_Byte;
static jclass J_Short;
static jclass J_Int;
static jclass J_Long;
static jclass J_Float;
static jclass J_Double;
static jclass J_String;
static jclass J_Timestamp;
static jclass J_TimestampTZ;
static jclass J_Decimal;

static jmethodID J_Bool_booleanValue;
static jmethodID J_Byte_byteValue;
static jmethodID J_Short_shortValue;
static jmethodID J_Int_intValue;
static jmethodID J_Long_longValue;
static jmethodID J_Float_floatValue;
static jmethodID J_Double_doubleValue;
static jmethodID J_Timestamp_getMicrosEpoch;
static jmethodID J_TimestampTZ_getMicrosEpoch;
static jmethodID J_Decimal_precision;
static jmethodID J_Decimal_scale;
static jmethodID J_Decimal_scaleByPowTen;
static jmethodID J_Decimal_toPlainString;
static jmethodID J_Decimal_longValue;

static jclass J_DuckResultSetMeta;
static jmethodID J_DuckResultSetMeta_init;

static jclass J_DuckVector;
static jmethodID J_DuckVector_init;
static jfieldID J_DuckVector_constlen;
static jfieldID J_DuckVector_varlen;

static jclass J_DuckArray;
static jmethodID J_DuckArray_init;

static jclass J_ByteBuffer;

static jmethodID J_Map_entrySet;
static jmethodID J_Set_iterator;
static jmethodID J_Iterator_hasNext;
static jmethodID J_Iterator_next;
static jmethodID J_Entry_getKey;
static jmethodID J_Entry_getValue;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
	// Get JNIEnv from vm
	JNIEnv *env;
	if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION) != JNI_OK) {
		return JNI_ERR;
	}

	jclass tmpLocalRef;

	tmpLocalRef = env->FindClass("java/nio/charset/Charset");
	J_Charset = (jclass)env->NewGlobalRef(tmpLocalRef);
	env->DeleteLocalRef(tmpLocalRef);

	jmethodID forName = env->GetStaticMethodID(J_Charset, "forName", "(Ljava/lang/String;)Ljava/nio/charset/Charset;");
	J_Charset_decode = env->GetMethodID(J_Charset, "decode", "(Ljava/nio/ByteBuffer;)Ljava/nio/CharBuffer;");
	jobject charset = env->CallStaticObjectMethod(J_Charset, forName, env->NewStringUTF("UTF-8"));
	J_Charset_UTF8 = env->NewGlobalRef(charset); // Prevent garbage collector from cleaning this up.

	tmpLocalRef = env->FindClass("java/nio/CharBuffer");
	J_CharBuffer = (jclass)env->NewGlobalRef(tmpLocalRef);
	env->DeleteLocalRef(tmpLocalRef);

	J_CharBuffer_toString = env->GetMethodID(J_CharBuffer, "toString", "()Ljava/lang/String;");

	tmpLocalRef = env->FindClass("java/sql/SQLException");
	J_SQLException = (jclass)env->NewGlobalRef(tmpLocalRef);
	env->DeleteLocalRef(tmpLocalRef);

	tmpLocalRef = env->FindClass("java/lang/Boolean");
	J_Bool = (jclass)env->NewGlobalRef(tmpLocalRef);
	env->DeleteLocalRef(tmpLocalRef);
	tmpLocalRef = env->FindClass("java/lang/Byte");
	J_Byte = (jclass)env->NewGlobalRef(tmpLocalRef);
	env->DeleteLocalRef(tmpLocalRef);
	tmpLocalRef = env->FindClass("java/lang/Short");
	J_Short = (jclass)env->NewGlobalRef(tmpLocalRef);
	env->DeleteLocalRef(tmpLocalRef);
	tmpLocalRef = env->FindClass("java/lang/Integer");
	J_Int = (jclass)env->NewGlobalRef(tmpLocalRef);
	env->DeleteLocalRef(tmpLocalRef);
	tmpLocalRef = env->FindClass("java/lang/Long");
	J_Long = (jclass)env->NewGlobalRef(tmpLocalRef);
	env->DeleteLocalRef(tmpLocalRef);
	tmpLocalRef = env->FindClass("java/lang/Float");
	J_Float = (jclass)env->NewGlobalRef(tmpLocalRef);
	env->DeleteLocalRef(tmpLocalRef);
	tmpLocalRef = env->FindClass("java/lang/Double");
	J_Double = (jclass)env->NewGlobalRef(tmpLocalRef);
	env->DeleteLocalRef(tmpLocalRef);
	tmpLocalRef = env->FindClass("java/lang/String");
	J_String = (jclass)env->NewGlobalRef(tmpLocalRef);
	env->DeleteLocalRef(tmpLocalRef);
	tmpLocalRef = env->FindClass("org/duckdb/DuckDBTimestamp");
	J_Timestamp = (jclass)env->NewGlobalRef(tmpLocalRef);
	env->DeleteLocalRef(tmpLocalRef);
	tmpLocalRef = env->FindClass("org/duckdb/DuckDBTimestampTZ");
	J_TimestampTZ = (jclass)env->NewGlobalRef(tmpLocalRef);
	env->DeleteLocalRef(tmpLocalRef);
	tmpLocalRef = env->FindClass("java/math/BigDecimal");
	J_Decimal = (jclass)env->NewGlobalRef(tmpLocalRef);
	env->DeleteLocalRef(tmpLocalRef);

	tmpLocalRef = env->FindClass("java/util/Map");
	J_Map_entrySet = env->GetMethodID(tmpLocalRef, "entrySet", "()Ljava/util/Set;");
	env->DeleteLocalRef(tmpLocalRef);

	tmpLocalRef = env->FindClass("java/util/Set");
	J_Set_iterator = env->GetMethodID(tmpLocalRef, "iterator", "()Ljava/util/Iterator;");
	env->DeleteLocalRef(tmpLocalRef);

	tmpLocalRef = env->FindClass("java/util/Iterator");
	J_Iterator_hasNext = env->GetMethodID(tmpLocalRef, "hasNext", "()Z");
	J_Iterator_next = env->GetMethodID(tmpLocalRef, "next", "()Ljava/lang/Object;");
	env->DeleteLocalRef(tmpLocalRef);

	tmpLocalRef = env->FindClass("org/duckdb/DuckDBArray");
	D_ASSERT(tmpLocalRef);
	J_DuckArray = (jclass)env->NewGlobalRef(tmpLocalRef);
	J_DuckArray_init = env->GetMethodID(J_DuckArray, "<init>", "(Lorg/duckdb/DuckDBVector;II)V");
	D_ASSERT(J_DuckArray_init);
	env->DeleteLocalRef(tmpLocalRef);

	tmpLocalRef = env->FindClass("java/util/Map$Entry");
	J_Entry_getKey = env->GetMethodID(tmpLocalRef, "getKey", "()Ljava/lang/Object;");
	J_Entry_getValue = env->GetMethodID(tmpLocalRef, "getValue", "()Ljava/lang/Object;");
	env->DeleteLocalRef(tmpLocalRef);

	J_Bool_booleanValue = env->GetMethodID(J_Bool, "booleanValue", "()Z");
	J_Byte_byteValue = env->GetMethodID(J_Byte, "byteValue", "()B");
	J_Short_shortValue = env->GetMethodID(J_Short, "shortValue", "()S");
	J_Int_intValue = env->GetMethodID(J_Int, "intValue", "()I");
	J_Long_longValue = env->GetMethodID(J_Long, "longValue", "()J");
	J_Float_floatValue = env->GetMethodID(J_Float, "floatValue", "()F");
	J_Double_doubleValue = env->GetMethodID(J_Double, "doubleValue", "()D");
	J_Timestamp_getMicrosEpoch = env->GetMethodID(J_Timestamp, "getMicrosEpoch", "()J");
	J_TimestampTZ_getMicrosEpoch = env->GetMethodID(J_TimestampTZ, "getMicrosEpoch", "()J");
	J_Decimal_precision = env->GetMethodID(J_Decimal, "precision", "()I");
	J_Decimal_scale = env->GetMethodID(J_Decimal, "scale", "()I");
	J_Decimal_scaleByPowTen = env->GetMethodID(J_Decimal, "scaleByPowerOfTen", "(I)Ljava/math/BigDecimal;");
	J_Decimal_toPlainString = env->GetMethodID(J_Decimal, "toPlainString", "()Ljava/lang/String;");
	J_Decimal_longValue = env->GetMethodID(J_Decimal, "longValue", "()J");

	tmpLocalRef = env->FindClass("org/duckdb/DuckDBResultSetMetaData");
	J_DuckResultSetMeta = (jclass)env->NewGlobalRef(tmpLocalRef);
	env->DeleteLocalRef(tmpLocalRef);

	J_DuckResultSetMeta_init =
	    env->GetMethodID(J_DuckResultSetMeta, "<init>",
	                     "(II[Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)V");

	tmpLocalRef = env->FindClass("org/duckdb/DuckDBVector");
	J_DuckVector = (jclass)env->NewGlobalRef(tmpLocalRef);
	env->DeleteLocalRef(tmpLocalRef);

	J_String_getBytes = env->GetMethodID(J_String, "getBytes", "(Ljava/nio/charset/Charset;)[B");

	J_DuckVector_init = env->GetMethodID(J_DuckVector, "<init>", "(Ljava/lang/String;I[Z)V");
	J_DuckVector_constlen = env->GetFieldID(J_DuckVector, "constlen_data", "Ljava/nio/ByteBuffer;");
	J_DuckVector_varlen = env->GetFieldID(J_DuckVector, "varlen_data", "[Ljava/lang/Object;");

	tmpLocalRef = env->FindClass("java/nio/ByteBuffer");
	J_ByteBuffer = (jclass)env->NewGlobalRef(tmpLocalRef);
	env->DeleteLocalRef(tmpLocalRef);

	return JNI_VERSION;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved) {
	// Get JNIEnv from vm
	JNIEnv *env;
	vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION);

	env->DeleteGlobalRef(J_Charset);
	env->DeleteGlobalRef(J_CharBuffer);
	env->DeleteGlobalRef(J_Charset_UTF8);
	env->DeleteGlobalRef(J_SQLException);
	env->DeleteGlobalRef(J_Bool);
	env->DeleteGlobalRef(J_Byte);
	env->DeleteGlobalRef(J_Short);
	env->DeleteGlobalRef(J_Int);
	env->DeleteGlobalRef(J_Long);
	env->DeleteGlobalRef(J_Float);
	env->DeleteGlobalRef(J_Double);
	env->DeleteGlobalRef(J_String);
	env->DeleteGlobalRef(J_Timestamp);
	env->DeleteGlobalRef(J_TimestampTZ);
	env->DeleteGlobalRef(J_Decimal);
	env->DeleteGlobalRef(J_DuckResultSetMeta);
	env->DeleteGlobalRef(J_DuckVector);
	env->DeleteGlobalRef(J_ByteBuffer);
}

static string byte_array_to_string(JNIEnv *env, jbyteArray ba_j) {
	idx_t len = env->GetArrayLength(ba_j);
	string ret;
	ret.resize(len);

	jbyte *bytes = (jbyte *)env->GetByteArrayElements(ba_j, NULL);

	for (idx_t i = 0; i < len; i++) {
		ret[i] = bytes[i];
	}
	env->ReleaseByteArrayElements(ba_j, bytes, 0);

	return ret;
}

static string jstring_to_string(JNIEnv *env, jstring string_j) {
	jbyteArray bytes = (jbyteArray)env->CallObjectMethod(string_j, J_String_getBytes, J_Charset_UTF8);
	return byte_array_to_string(env, bytes);
}

static jobject decode_charbuffer_to_jstring(JNIEnv *env, const char *d_str, idx_t d_str_len) {
	auto bb = env->NewDirectByteBuffer((void *)d_str, d_str_len);
	auto j_cb = env->CallObjectMethod(J_Charset_UTF8, J_Charset_decode, bb);
	auto j_str = env->CallObjectMethod(j_cb, J_CharBuffer_toString);
	return j_str;
}

/**
 * Associates a duckdb::Connection with a duckdb::DuckDB. The DB may be shared amongst many ConnectionHolders, but the
 * Connection is unique to this holder. Every Java DuckDBConnection has exactly 1 of these holders, and they are never
 * shared. The holder is freed when the DuckDBConnection is closed. When the last holder sharing a DuckDB is freed, the
 * DuckDB is released as well.
 */
struct ConnectionHolder {
	const shared_ptr<duckdb::DuckDB> db;
	const duckdb::unique_ptr<duckdb::Connection> connection;

	ConnectionHolder(shared_ptr<duckdb::DuckDB> _db) : db(_db), connection(make_uniq<duckdb::Connection>(*_db)) {
	}
};

/**
 * Throws a SQLException and returns nullptr if a valid Connection can't be retrieved from the buffer.
 */
static Connection *get_connection(JNIEnv *env, jobject conn_ref_buf) {
	if (!conn_ref_buf) {
		env->ThrowNew(J_SQLException, "Invalid connection");
		return nullptr;
	}
	auto conn_holder = (ConnectionHolder *)env->GetDirectBufferAddress(conn_ref_buf);
	if (!conn_holder) {
		env->ThrowNew(J_SQLException, "Invalid connection");
		return nullptr;
	}
	auto conn_ref = conn_holder->connection.get();
	if (!conn_ref || !conn_ref->context) {
		env->ThrowNew(J_SQLException, "Invalid connection");
		return nullptr;
	}

	return conn_ref;
}

//! The database instance cache, used so that multiple connections to the same file point to the same database object
duckdb::DBInstanceCache instance_cache;

JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1startup(JNIEnv *env, jclass, jbyteArray database_j,
                                                                             jboolean read_only, jobject props) {
	auto database = byte_array_to_string(env, database_j);
	DBConfig config;
	if (read_only) {
		config.options.access_mode = AccessMode::READ_ONLY;
	}
	try {
		jobject entry_set = env->CallObjectMethod(props, J_Map_entrySet);
		jobject iterator = env->CallObjectMethod(entry_set, J_Set_iterator);

		while (env->CallBooleanMethod(iterator, J_Iterator_hasNext)) {
			jobject pair = env->CallObjectMethod(iterator, J_Iterator_next);
			jobject key = env->CallObjectMethod(pair, J_Entry_getKey);
			jobject value = env->CallObjectMethod(pair, J_Entry_getValue);

			D_ASSERT(env->IsInstanceOf(key, J_String));
			const string &key_str = jstring_to_string(env, (jstring)key);

			D_ASSERT(env->IsInstanceOf(value, J_String));
			const string &value_str = jstring_to_string(env, (jstring)value);

			auto const pOption = DBConfig::GetOptionByName(key_str);
			if (pOption) {
				config.SetOption(*pOption, Value(value_str));
			} else {
				throw CatalogException(
				    "unrecognized configuration parameter \"%s\"\n%s", key_str,
				    StringUtil::CandidatesErrorMessage(DBConfig::GetOptionNames(), key_str, "Did you mean"));
			}
		}
		bool cache_instance = database != ":memory:" && !database.empty();
		auto shared_db = instance_cache.GetOrCreateInstance(database, config, cache_instance);
		auto conn_holder = new ConnectionHolder(shared_db);

		return env->NewDirectByteBuffer(conn_holder, 0);
	} catch (exception &e) {
		env->ThrowNew(J_SQLException, e.what());
		return nullptr;
	}
	return nullptr;
}

JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1connect(JNIEnv *env, jclass,
                                                                             jobject conn_ref_buf) {
	auto conn_ref = (ConnectionHolder *)env->GetDirectBufferAddress(conn_ref_buf);
	try {
		auto conn = new ConnectionHolder(conn_ref->db);
		return env->NewDirectByteBuffer(conn, 0);
	} catch (exception &e) {
		env->ThrowNew(J_SQLException, e.what());
		return nullptr;
	}
	return nullptr;
}

JNIEXPORT jstring JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1get_1schema(JNIEnv *env, jclass,
                                                                                 jobject conn_ref_buf) {
	auto conn_ref = get_connection(env, conn_ref_buf);
	if (!conn_ref) {
		return nullptr;
	}

	auto entry = ClientData::Get(*conn_ref->context).catalog_search_path->GetDefault();

	return env->NewStringUTF(entry.schema.c_str());
}

static void set_catalog_search_path(JNIEnv *env, jobject conn_ref_buf, CatalogSearchEntry search_entry,
                                    bool is_set_schema) {
	auto conn_ref = get_connection(env, conn_ref_buf);
	if (!conn_ref) {
		return;
	}

	try {
		conn_ref->context->RunFunctionInTransaction(
		    [&]() { ClientData::Get(*conn_ref->context).catalog_search_path->Set(search_entry, is_set_schema); });
	} catch (const exception &e) {
		env->ThrowNew(J_SQLException, e.what());
	}
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1set_1schema(JNIEnv *env, jclass, jobject conn_ref_buf,
                                                                              jstring schema) {
	set_catalog_search_path(env, conn_ref_buf, CatalogSearchEntry(INVALID_CATALOG, jstring_to_string(env, schema)),
	                        true);
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1set_1catalog(JNIEnv *env, jclass,
                                                                               jobject conn_ref_buf, jstring catalog) {
	set_catalog_search_path(env, conn_ref_buf, CatalogSearchEntry(jstring_to_string(env, catalog), DEFAULT_SCHEMA),
	                        false);
}

JNIEXPORT jstring JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1get_1catalog(JNIEnv *env, jclass,
                                                                                  jobject conn_ref_buf) {
	auto conn_ref = get_connection(env, conn_ref_buf);
	if (!conn_ref) {
		return nullptr;
	}

	auto entry = ClientData::Get(*conn_ref->context).catalog_search_path->GetDefault();
	if (entry.catalog == INVALID_CATALOG) {
		entry.catalog = DatabaseManager::GetDefaultDatabase(*conn_ref->context);
	}

	return env->NewStringUTF(entry.catalog.c_str());
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1set_1auto_1commit(JNIEnv *env, jclass,
                                                                                    jobject conn_ref_buf,
                                                                                    jboolean auto_commit) {
	auto conn_ref = get_connection(env, conn_ref_buf);
	if (!conn_ref) {
		return;
	}
	conn_ref->context->RunFunctionInTransaction([&]() { conn_ref->SetAutoCommit(auto_commit); });
}

JNIEXPORT jboolean JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1get_1auto_1commit(JNIEnv *env, jclass,
                                                                                        jobject conn_ref_buf) {
	auto conn_ref = get_connection(env, conn_ref_buf);
	if (!conn_ref) {
		return false;
	}
	return conn_ref->IsAutoCommit();
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1interrupt(JNIEnv *env, jclass, jobject conn_ref_buf) {
	auto conn_ref = get_connection(env, conn_ref_buf);
	if (!conn_ref) {
		return;
	}
	conn_ref->Interrupt();
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1disconnect(JNIEnv *env, jclass,
                                                                             jobject conn_ref_buf) {
	auto conn_ref = (ConnectionHolder *)env->GetDirectBufferAddress(conn_ref_buf);
	if (conn_ref) {
		delete conn_ref;
	}
}

struct StatementHolder {
	duckdb::unique_ptr<PreparedStatement> stmt;
};

#include "utf8proc_wrapper.hpp"

JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1prepare(JNIEnv *env, jclass, jobject conn_ref_buf,
                                                                             jbyteArray query_j) {
	auto conn_ref = get_connection(env, conn_ref_buf);
	if (!conn_ref) {
		return nullptr;
	}

	auto query = byte_array_to_string(env, query_j);

	// invalid sql raises a parse exception
	// need to be caught and thrown via JNI
	duckdb::vector<duckdb::unique_ptr<SQLStatement>> statements;
	try {
		statements = conn_ref->ExtractStatements(query.c_str());
	} catch (const std::exception &e) {
		env->ThrowNew(J_SQLException, e.what());
		return nullptr;
	}

	if (statements.empty()) {
		env->ThrowNew(J_SQLException, "No statements to execute.");
		return nullptr;
	}

	// if there are multiple statements, we directly execute the statements besides the last one
	// we only return the result of the last statement to the user, unless one of the previous statements fails
	for (idx_t i = 0; i + 1 < statements.size(); i++) {
		try {
			auto res = conn_ref->Query(std::move(statements[i]));
			if (res->HasError()) {
				env->ThrowNew(J_SQLException, res->GetError().c_str());
				return nullptr;
			}
		} catch (const std::exception &ex) {
			env->ThrowNew(J_SQLException, ex.what());
			return nullptr;
		}
	}

	auto stmt_ref = new StatementHolder();
	stmt_ref->stmt = conn_ref->Prepare(std::move(statements.back()));
	if (stmt_ref->stmt->HasError()) {
		string error_msg = string(stmt_ref->stmt->GetError());
		stmt_ref->stmt = nullptr;

		// No success, so it must be deleted
		delete stmt_ref;
		env->ThrowNew(J_SQLException, error_msg.c_str());

		// Just return control flow back to JVM, as an Exception is pending anyway
		return nullptr;
	}
	return env->NewDirectByteBuffer(stmt_ref, 0);
}

struct ResultHolder {
	duckdb::unique_ptr<QueryResult> res;
	duckdb::unique_ptr<DataChunk> chunk;
};

JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1execute(JNIEnv *env, jclass, jobject stmt_ref_buf,
                                                                             jobjectArray params) {
	auto stmt_ref = (StatementHolder *)env->GetDirectBufferAddress(stmt_ref_buf);
	if (!stmt_ref) {
		env->ThrowNew(J_SQLException, "Invalid statement");
		return nullptr;
	}
	auto res_ref = make_uniq<ResultHolder>();
	duckdb::vector<Value> duckdb_params;

	idx_t param_len = env->GetArrayLength(params);
	if (param_len != stmt_ref->stmt->n_param) {
		env->ThrowNew(J_SQLException, "Parameter count mismatch");
		return nullptr;
	}

	if (param_len > 0) {
		for (idx_t i = 0; i < param_len; i++) {
			auto param = env->GetObjectArrayElement(params, i);
			if (param == nullptr) {
				duckdb_params.push_back(Value());
				continue;
			} else if (env->IsInstanceOf(param, J_Bool)) {
				duckdb_params.push_back(Value::BOOLEAN(env->CallBooleanMethod(param, J_Bool_booleanValue)));
				continue;
			} else if (env->IsInstanceOf(param, J_Byte)) {
				duckdb_params.push_back(Value::TINYINT(env->CallByteMethod(param, J_Byte_byteValue)));
				continue;
			} else if (env->IsInstanceOf(param, J_Short)) {
				duckdb_params.push_back(Value::SMALLINT(env->CallShortMethod(param, J_Short_shortValue)));
				continue;
			} else if (env->IsInstanceOf(param, J_Int)) {
				duckdb_params.push_back(Value::INTEGER(env->CallIntMethod(param, J_Int_intValue)));
				continue;
			} else if (env->IsInstanceOf(param, J_Long)) {
				duckdb_params.push_back(Value::BIGINT(env->CallLongMethod(param, J_Long_longValue)));
				continue;
			} else if (env->IsInstanceOf(param, J_TimestampTZ)) { // Check for subclass before superclass!
				duckdb_params.push_back(
				    Value::TIMESTAMPTZ((timestamp_t)env->CallLongMethod(param, J_TimestampTZ_getMicrosEpoch)));
				continue;
			} else if (env->IsInstanceOf(param, J_Timestamp)) {
				duckdb_params.push_back(
				    Value::TIMESTAMP((timestamp_t)env->CallLongMethod(param, J_Timestamp_getMicrosEpoch)));
				continue;
			} else if (env->IsInstanceOf(param, J_Float)) {
				duckdb_params.push_back(Value::FLOAT(env->CallFloatMethod(param, J_Float_floatValue)));
				continue;
			} else if (env->IsInstanceOf(param, J_Double)) {
				duckdb_params.push_back(Value::DOUBLE(env->CallDoubleMethod(param, J_Double_doubleValue)));
				continue;
			} else if (env->IsInstanceOf(param, J_Decimal)) {
				jint precision = env->CallIntMethod(param, J_Decimal_precision);
				jint scale = env->CallIntMethod(param, J_Decimal_scale);

				// Java BigDecimal type can have scale that exceeds the precision
				// Which our DECIMAL type does not support (assert(width >= scale))
				if (scale > precision) {
					precision = scale;
				}

				// DECIMAL scale is unsigned, so negative values are not supported
				if (scale < 0) {
					env->ThrowNew(J_SQLException, "Converting from a BigDecimal with negative scale is not supported");
					return nullptr;
				}

				if (precision <= 18) { // normal sizes -> avoid string processing
					jobject no_point_dec = env->CallObjectMethod(param, J_Decimal_scaleByPowTen, scale);
					jlong result = env->CallLongMethod(no_point_dec, J_Decimal_longValue);
					duckdb_params.push_back(Value::DECIMAL((int64_t)result, (uint8_t)precision, (uint8_t)scale));
				} else if (precision <= 38) { // larger than int64 -> get string and cast
					jobject str_val = env->CallObjectMethod(param, J_Decimal_toPlainString);
					auto *str_char = env->GetStringUTFChars((jstring)str_val, 0);
					Value val = Value(str_char);
					val = val.DefaultCastAs(LogicalType::DECIMAL(precision, scale));

					duckdb_params.push_back(val);
					env->ReleaseStringUTFChars((jstring)str_val, str_char);
				}
				continue;
			} else if (env->IsInstanceOf(param, J_String)) {
				auto param_string = jstring_to_string(env, (jstring)param);
				try {
					duckdb_params.push_back(Value(param_string));
				} catch (Exception const &e) {
					env->ThrowNew(J_SQLException, e.what());
					return nullptr;
				}
				continue;
			} else {
				env->ThrowNew(J_SQLException, "Unsupported parameter type");
				return nullptr;
			}
		}
	}

	res_ref->res = stmt_ref->stmt->Execute(duckdb_params, false);
	if (res_ref->res->HasError()) {
		string error_msg = string(res_ref->res->GetError());
		res_ref->res = nullptr;
		env->ThrowNew(J_SQLException, error_msg.c_str());
		return nullptr;
	}
	return env->NewDirectByteBuffer(res_ref.release(), 0);
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1release(JNIEnv *env, jclass, jobject stmt_ref_buf) {
	auto stmt_ref = (StatementHolder *)env->GetDirectBufferAddress(stmt_ref_buf);
	if (stmt_ref) {
		delete stmt_ref;
	}
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1free_1result(JNIEnv *env, jclass,
                                                                               jobject res_ref_buf) {
	auto res_ref = (ResultHolder *)env->GetDirectBufferAddress(res_ref_buf);
	if (res_ref) {
		delete res_ref;
	}
}

static std::string type_to_jduckdb_type(LogicalType logical_type) {
	switch (logical_type.id()) {
	case LogicalTypeId::DECIMAL: {

		uint8_t width = 0;
		uint8_t scale = 0;
		logical_type.GetDecimalProperties(width, scale);
		std::string width_scale = std::to_string(width) + std::string(";") + std::to_string(scale);

		auto physical_type = logical_type.InternalType();
		switch (physical_type) {
		case PhysicalType::INT16: {
			string res = std::string("DECIMAL16;") + width_scale;
			return res;
		}
		case PhysicalType::INT32: {
			string res = std::string("DECIMAL32;") + width_scale;
			return res;
		}
		case PhysicalType::INT64: {
			string res = std::string("DECIMAL64;") + width_scale;
			return res;
		}
		case PhysicalType::INT128: {
			string res = std::string("DECIMAL128;") + width_scale;
			return res;
		}
		default:
			return std::string("no physical type found");
		}
	} break;
	default:
		return logical_type.ToString();
	}
}

JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1meta(JNIEnv *env, jclass, jobject stmt_ref_buf) {

	auto stmt_ref = (StatementHolder *)env->GetDirectBufferAddress(stmt_ref_buf);
	if (!stmt_ref || !stmt_ref->stmt || stmt_ref->stmt->HasError()) {
		env->ThrowNew(J_SQLException, "Invalid statement");
		return nullptr;
	}

	auto column_count = stmt_ref->stmt->ColumnCount();
	auto &names = stmt_ref->stmt->GetNames();
	auto &types = stmt_ref->stmt->GetTypes();

	auto name_array = env->NewObjectArray(column_count, J_String, nullptr);
	auto type_array = env->NewObjectArray(column_count, J_String, nullptr);
	auto type_detail_array = env->NewObjectArray(column_count, J_String, nullptr);

	for (idx_t col_idx = 0; col_idx < column_count; col_idx++) {
		std::string col_name;
		if (types[col_idx].id() == LogicalTypeId::ENUM) {
			col_name = "ENUM";
		} else {
			col_name = types[col_idx].ToString();
		}

		env->SetObjectArrayElement(name_array, col_idx,
		                           decode_charbuffer_to_jstring(env, names[col_idx].c_str(), names[col_idx].length()));
		env->SetObjectArrayElement(type_array, col_idx, env->NewStringUTF(col_name.c_str()));
		env->SetObjectArrayElement(type_detail_array, col_idx,
		                           env->NewStringUTF(type_to_jduckdb_type(types[col_idx]).c_str()));
	}

	auto return_type =
	    env->NewStringUTF(StatementReturnTypeToString(stmt_ref->stmt->GetStatementProperties().return_type).c_str());

	return env->NewObject(J_DuckResultSetMeta, J_DuckResultSetMeta_init, stmt_ref->stmt->n_param, column_count,
	                      name_array, type_array, type_detail_array, return_type);
}

jobject ProcessVector(JNIEnv *env, Connection *conn_ref, Vector &vec, idx_t row_count);

JNIEXPORT jobjectArray JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1fetch(JNIEnv *env, jclass,
                                                                                jobject res_ref_buf,
                                                                                jobject conn_ref_buf) {
	auto res_ref = (ResultHolder *)env->GetDirectBufferAddress(res_ref_buf);
	if (!res_ref || !res_ref->res || res_ref->res->HasError()) {
		env->ThrowNew(J_SQLException, "Invalid result set");
		return nullptr;
	}

	auto conn_ref = get_connection(env, conn_ref_buf);
	if (conn_ref == nullptr) {
		return nullptr;
	}

	res_ref->chunk = res_ref->res->Fetch();
	if (!res_ref->chunk) {
		res_ref->chunk = make_uniq<DataChunk>();
	}
	auto row_count = res_ref->chunk->size();
	auto vec_array = (jobjectArray)env->NewObjectArray(res_ref->chunk->ColumnCount(), J_DuckVector, nullptr);

	for (idx_t col_idx = 0; col_idx < res_ref->chunk->ColumnCount(); col_idx++) {
		auto &vec = res_ref->chunk->data[col_idx];

		auto jvec = ProcessVector(env, conn_ref, vec, row_count);

		env->SetObjectArrayElement(vec_array, col_idx, jvec);
	}

	return vec_array;
}
jobject ProcessVector(JNIEnv *env, Connection *conn_ref, Vector &vec, idx_t row_count) {
	auto type_str = env->NewStringUTF(type_to_jduckdb_type(vec.GetType()).c_str());
	// construct nullmask
	auto null_array = env->NewBooleanArray(row_count);
	jboolean *null_array_ptr = env->GetBooleanArrayElements(null_array, nullptr);
	for (idx_t row_idx = 0; row_idx < row_count; row_idx++) {
		null_array_ptr[row_idx] = FlatVector::IsNull(vec, row_idx);
	}
	env->ReleaseBooleanArrayElements(null_array, null_array_ptr, 0);

	auto jvec = env->NewObject(J_DuckVector, J_DuckVector_init, type_str, (int)row_count, null_array);

	jobject constlen_data = nullptr;
	jobjectArray varlen_data = nullptr;

	switch (vec.GetType().id()) {
	case LogicalTypeId::BOOLEAN:
		constlen_data = env->NewDirectByteBuffer(FlatVector::GetData(vec), row_count * sizeof(bool));
		break;
	case LogicalTypeId::TINYINT:
		constlen_data = env->NewDirectByteBuffer(FlatVector::GetData(vec), row_count * sizeof(int8_t));
		break;
	case LogicalTypeId::SMALLINT:
		constlen_data = env->NewDirectByteBuffer(FlatVector::GetData(vec), row_count * sizeof(int16_t));
		break;
	case LogicalTypeId::INTEGER:
		constlen_data = env->NewDirectByteBuffer(FlatVector::GetData(vec), row_count * sizeof(int32_t));
		break;
	case LogicalTypeId::BIGINT:
		constlen_data = env->NewDirectByteBuffer(FlatVector::GetData(vec), row_count * sizeof(int64_t));
		break;
	case LogicalTypeId::UTINYINT:
		constlen_data = env->NewDirectByteBuffer(FlatVector::GetData(vec), row_count * sizeof(uint8_t));
		break;
	case LogicalTypeId::USMALLINT:
		constlen_data = env->NewDirectByteBuffer(FlatVector::GetData(vec), row_count * sizeof(uint16_t));
		break;
	case LogicalTypeId::UINTEGER:
		constlen_data = env->NewDirectByteBuffer(FlatVector::GetData(vec), row_count * sizeof(uint32_t));
		break;
	case LogicalTypeId::UBIGINT:
		constlen_data = env->NewDirectByteBuffer(FlatVector::GetData(vec), row_count * sizeof(uint64_t));
		break;
	case LogicalTypeId::HUGEINT:
		constlen_data = env->NewDirectByteBuffer(FlatVector::GetData(vec), row_count * sizeof(hugeint_t));
		break;
	case LogicalTypeId::FLOAT:
		constlen_data = env->NewDirectByteBuffer(FlatVector::GetData(vec), row_count * sizeof(float));
		break;
	case LogicalTypeId::DECIMAL: {
		auto physical_type = vec.GetType().InternalType();

		switch (physical_type) {
		case PhysicalType::INT16:
			constlen_data = env->NewDirectByteBuffer(FlatVector::GetData(vec), row_count * sizeof(int16_t));
			break;
		case PhysicalType::INT32:
			constlen_data = env->NewDirectByteBuffer(FlatVector::GetData(vec), row_count * sizeof(int32_t));
			break;
		case PhysicalType::INT64:
			constlen_data = env->NewDirectByteBuffer(FlatVector::GetData(vec), row_count * sizeof(int64_t));
			break;
		case PhysicalType::INT128:
			constlen_data = env->NewDirectByteBuffer(FlatVector::GetData(vec), row_count * sizeof(hugeint_t));
			break;
		default:
			throw InternalException("Unimplemented physical type for decimal");
		}
		break;
	}
	case LogicalTypeId::DOUBLE:
		constlen_data = env->NewDirectByteBuffer(FlatVector::GetData(vec), row_count * sizeof(double));
		break;
	case LogicalTypeId::TIME_TZ:
	case LogicalTypeId::TIMESTAMP_SEC:
	case LogicalTypeId::TIMESTAMP_MS:
	case LogicalTypeId::TIMESTAMP:
	case LogicalTypeId::TIMESTAMP_NS:
	case LogicalTypeId::TIMESTAMP_TZ:
		constlen_data = env->NewDirectByteBuffer(FlatVector::GetData(vec), row_count * sizeof(timestamp_t));
		break;
	case LogicalTypeId::ENUM:
		varlen_data = env->NewObjectArray(row_count, J_String, nullptr);
		for (idx_t row_idx = 0; row_idx < row_count; row_idx++) {
			if (FlatVector::IsNull(vec, row_idx)) {
				continue;
			}
			auto d_str = vec.GetValue(row_idx).ToString();
			jstring j_str = env->NewStringUTF(d_str.c_str());
			env->SetObjectArrayElement(varlen_data, row_idx, j_str);
		}
		break;
	case LogicalTypeId::BLOB:
		varlen_data = env->NewObjectArray(row_count, J_ByteBuffer, nullptr);

		for (idx_t row_idx = 0; row_idx < row_count; row_idx++) {
			if (FlatVector::IsNull(vec, row_idx)) {
				continue;
			}
			auto &d_str = ((string_t *)FlatVector::GetData(vec))[row_idx];
			auto j_obj = env->NewDirectByteBuffer((void *)d_str.GetData(), d_str.GetSize());
			env->SetObjectArrayElement(varlen_data, row_idx, j_obj);
		}
		break;
	case LogicalTypeId::UUID:
		constlen_data = env->NewDirectByteBuffer(FlatVector::GetData(vec), row_count * sizeof(hugeint_t));
		break;
	case LogicalTypeId::LIST: {
		varlen_data = env->NewObjectArray(row_count, J_DuckArray, nullptr);

		auto list_entries = FlatVector::GetData<list_entry_t>(vec);

		auto list_size = ListVector::GetListSize(vec);
		auto &list_vector = ListVector::GetEntry(vec);
		auto j_vec = ProcessVector(env, conn_ref, list_vector, list_size);

		for (idx_t row_idx = 0; row_idx < row_count; row_idx++) {
			if (FlatVector::IsNull(vec, row_idx)) {
				continue;
			}

			auto offset = list_entries[row_idx].offset;
			auto limit = list_entries[row_idx].length;

			auto j_obj = env->NewObject(J_DuckArray, J_DuckArray_init, j_vec, offset, limit);

			env->SetObjectArrayElement(varlen_data, row_idx, j_obj);
		}
		break;
	}
	default: {
		Vector string_vec(LogicalType::VARCHAR);
		VectorOperations::Cast(*conn_ref->context, vec, string_vec, row_count);
		vec.ReferenceAndSetType(string_vec);
		// fall through on purpose
	}
	case LogicalTypeId::VARCHAR:
		varlen_data = env->NewObjectArray(row_count, J_String, nullptr);
		for (idx_t row_idx = 0; row_idx < row_count; row_idx++) {
			if (FlatVector::IsNull(vec, row_idx)) {
				continue;
			}
			auto d_str = ((string_t *)FlatVector::GetData(vec))[row_idx];
			auto j_str = decode_charbuffer_to_jstring(env, d_str.GetData(), d_str.GetSize());
			env->SetObjectArrayElement(varlen_data, row_idx, j_str);
		}
		break;
	}

	env->SetObjectField(jvec, J_DuckVector_constlen, constlen_data);
	env->SetObjectField(jvec, J_DuckVector_varlen, varlen_data);

	return jvec;
}

JNIEXPORT jint JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1fetch_1size(JNIEnv *, jclass) {
	return STANDARD_VECTOR_SIZE;
}

JNIEXPORT jobject JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1create_1appender(JNIEnv *env, jclass,
                                                                                      jobject conn_ref_buf,
                                                                                      jbyteArray schema_name_j,
                                                                                      jbyteArray table_name_j) {

	auto conn_ref = get_connection(env, conn_ref_buf);
	if (!conn_ref) {
		return nullptr;
	}
	auto schema_name = byte_array_to_string(env, schema_name_j);
	auto table_name = byte_array_to_string(env, table_name_j);
	try {
		auto appender = new Appender(*conn_ref, schema_name, table_name);
		return env->NewDirectByteBuffer(appender, 0);
	} catch (exception &e) {
		env->ThrowNew(J_SQLException, e.what());
		return nullptr;
	}
	return nullptr;
}

static Appender *get_appender(JNIEnv *env, jobject appender_ref_buf) {
	auto appender_ref = (Appender *)env->GetDirectBufferAddress(appender_ref_buf);
	if (!appender_ref) {
		env->ThrowNew(J_SQLException, "Invalid appender");
		return nullptr;
	}
	return appender_ref;
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1appender_1begin_1row(JNIEnv *env, jclass,
                                                                                       jobject appender_ref_buf) {
	try {
		get_appender(env, appender_ref_buf)->BeginRow();
	} catch (exception &e) {
		env->ThrowNew(J_SQLException, e.what());
		return;
	}
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1appender_1end_1row(JNIEnv *env, jclass,
                                                                                     jobject appender_ref_buf) {
	try {
		get_appender(env, appender_ref_buf)->EndRow();
	} catch (exception &e) {
		env->ThrowNew(J_SQLException, e.what());
		return;
	}
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1appender_1flush(JNIEnv *env, jclass,
                                                                                  jobject appender_ref_buf) {
	try {
		get_appender(env, appender_ref_buf)->Flush();
	} catch (exception &e) {
		env->ThrowNew(J_SQLException, e.what());
		return;
	}
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1appender_1close(JNIEnv *env, jclass,
                                                                                  jobject appender_ref_buf) {
	try {
		auto appender = get_appender(env, appender_ref_buf);
		appender->Close();
		delete appender;
	} catch (exception &e) {
		env->ThrowNew(J_SQLException, e.what());
		return;
	}
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1appender_1append_1boolean(JNIEnv *env, jclass,
                                                                                            jobject appender_ref_buf,
                                                                                            jboolean value) {
	try {
		get_appender(env, appender_ref_buf)->Append((bool)value);
	} catch (exception &e) {
		env->ThrowNew(J_SQLException, e.what());
		return;
	}
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1appender_1append_1byte(JNIEnv *env, jclass,
                                                                                         jobject appender_ref_buf,
                                                                                         jbyte value) {
	try {
		get_appender(env, appender_ref_buf)->Append((int8_t)value);
	} catch (exception &e) {
		env->ThrowNew(J_SQLException, e.what());
		return;
	}
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1appender_1append_1short(JNIEnv *env, jclass,
                                                                                          jobject appender_ref_buf,
                                                                                          jshort value) {
	try {
		get_appender(env, appender_ref_buf)->Append((int16_t)value);
	} catch (exception &e) {
		env->ThrowNew(J_SQLException, e.what());
		return;
	}
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1appender_1append_1int(JNIEnv *env, jclass,
                                                                                        jobject appender_ref_buf,
                                                                                        jint value) {
	try {
		get_appender(env, appender_ref_buf)->Append((int32_t)value);
	} catch (exception &e) {
		env->ThrowNew(J_SQLException, e.what());
		return;
	}
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1appender_1append_1long(JNIEnv *env, jclass,
                                                                                         jobject appender_ref_buf,
                                                                                         jlong value) {
	try {
		get_appender(env, appender_ref_buf)->Append((int64_t)value);
	} catch (exception &e) {
		env->ThrowNew(J_SQLException, e.what());
		return;
	}
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1appender_1append_1float(JNIEnv *env, jclass,
                                                                                          jobject appender_ref_buf,
                                                                                          jfloat value) {
	try {
		get_appender(env, appender_ref_buf)->Append((float)value);
	} catch (exception &e) {
		env->ThrowNew(J_SQLException, e.what());
		return;
	}
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1appender_1append_1double(JNIEnv *env, jclass,
                                                                                           jobject appender_ref_buf,
                                                                                           jdouble value) {
	try {
		get_appender(env, appender_ref_buf)->Append((double)value);
	} catch (exception &e) {
		env->ThrowNew(J_SQLException, e.what());
		return;
	}
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1appender_1append_1string(JNIEnv *env, jclass,
                                                                                           jobject appender_ref_buf,
                                                                                           jbyteArray value) {
	try {
		if (env->IsSameObject(value, NULL)) {
			get_appender(env, appender_ref_buf)->Append<std::nullptr_t>(nullptr);
			return;
		}

		auto string_value = byte_array_to_string(env, value);
		get_appender(env, appender_ref_buf)->Append(string_value.c_str());
	} catch (exception &e) {
		env->ThrowNew(J_SQLException, e.what());
		return;
	}
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1appender_1append_1null(JNIEnv *env, jclass,
                                                                                         jobject appender_ref_buf) {
	try {
		get_appender(env, appender_ref_buf)->Append<std::nullptr_t>(nullptr);
		return;
	} catch (exception &e) {
		env->ThrowNew(J_SQLException, e.what());
		return;
	}
}

JNIEXPORT jlong JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1arrow_1stream(JNIEnv *env, jclass,
                                                                                 jobject res_ref_buf,
                                                                                 jlong batch_size) {

	auto res_ref = (ResultHolder *)env->GetDirectBufferAddress(res_ref_buf);
	if (!res_ref || !res_ref->res || res_ref->res->HasError()) {
		env->ThrowNew(J_SQLException, "Invalid result set");
	}

	auto wrapper = new ResultArrowArrayStreamWrapper(std::move(res_ref->res), batch_size);
	return (jlong)&wrapper->stream;
}

class JavaArrowTabularStreamFactory {
public:
	JavaArrowTabularStreamFactory(ArrowArrayStream *stream_ptr_p) : stream_ptr(stream_ptr_p) {};

	static duckdb::unique_ptr<ArrowArrayStreamWrapper> Produce(uintptr_t factory_p, ArrowStreamParameters &parameters) {

		auto factory = (JavaArrowTabularStreamFactory *)factory_p;
		if (!factory->stream_ptr->release) {
			throw InvalidInputException("This stream has been released");
		}
		auto res = make_uniq<ArrowArrayStreamWrapper>();
		res->arrow_array_stream = *factory->stream_ptr;
		factory->stream_ptr->release = nullptr;
		return res;
	}

	static void GetSchema(uintptr_t factory_p, ArrowSchemaWrapper &schema) {
		auto factory = (JavaArrowTabularStreamFactory *)factory_p;
		if (!factory->stream_ptr->release) {
			throw InvalidInputException("This stream has been released");
		}
		factory->stream_ptr->get_schema(factory->stream_ptr, &schema.arrow_schema);
		return;
	}

	ArrowArrayStream *stream_ptr;
};

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1arrow_1register(JNIEnv *env, jclass,
                                                                                  jobject conn_ref_buf,
                                                                                  jlong arrow_array_stream_pointer,
                                                                                  jbyteArray name_j) {

	auto conn = get_connection(env, conn_ref_buf);
	auto name = byte_array_to_string(env, name_j);

	auto arrow_array_stream = (ArrowArrayStream *)(uintptr_t)arrow_array_stream_pointer;

	auto factory = new JavaArrowTabularStreamFactory(arrow_array_stream);
	duckdb::vector<Value> parameters;
	parameters.push_back(Value::POINTER((uintptr_t)factory));
	parameters.push_back(Value::POINTER((uintptr_t)JavaArrowTabularStreamFactory::Produce));
	parameters.push_back(Value::POINTER((uintptr_t)JavaArrowTabularStreamFactory::GetSchema));
	conn->TableFunction("arrow_scan_dumb", parameters)->CreateView(name, true, true);
}

JNIEXPORT void JNICALL Java_org_duckdb_DuckDBNative_duckdb_1jdbc_1create_1extension_1type(JNIEnv *env, jclass,
                                                                                          jobject conn_buf) {

	auto connection = get_connection(env, conn_buf);
	if (!connection) {
		return;
	}

	connection->BeginTransaction();

	child_list_t<LogicalType> children = {{"hello", LogicalType::VARCHAR}, {"world", LogicalType::VARCHAR}};
	auto id = LogicalType::STRUCT(children);
	auto type_name = "test_type";
	id.SetAlias(type_name);
	CreateTypeInfo info(type_name, id);

	auto &catalog_name = DatabaseManager::GetDefaultDatabase(*connection->context);
	auto &catalog = Catalog::GetCatalog(*connection->context, catalog_name);
	catalog.CreateType(*connection->context, info);

	connection->Commit();
}
