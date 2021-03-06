#include "unpacker.h"

VALUE cAsciiPack_Unpacker;

static VALUE Unpacker_buffer_read(unpacker_t* ptr);

static void
unpacker_mark(unpacker_t* ptr)
{
}

static VALUE
Unpacker_alloc(VALUE klass)
{
	unpacker_t *ptr = ALLOC(unpacker_t);
	return Data_Wrap_Struct(klass, unpacker_mark, -1, ptr);
}

static VALUE
Unpacker_init(VALUE self, VALUE obj, int argc, VALUE *argv, VALUE size)
{
	UNPACKER(self, ptr);

	if (!ptr) {
		rb_raise(rb_eArgError, "unallocated unpacker");
	}

	ptr->buffer = NULL;
	ptr->ch = NULL;

	return self;
}

static VALUE
Unpacker_initialize(int argc, VALUE *argv, VALUE self)
{
	VALUE obj = *argv++;
	VALUE size = Qnil;
	return Unpacker_init(self, obj, argc, argv, size);
}

static unsigned short
to_i16 (char ch)
{
	unsigned short ret = 0;
	if ('0' <= ch && ch <= '9') {
		ret += ch - '0';
	} else if ('a' <= ch && ch <= 'f') {
		ret += ch - 'a' + 10;
	}
	return ret;
}

static uint64_t
to_i16all (unpacker_t* ptr, int len)
{
	uint64_t ret = 0;
	while (len--) {
		ret += to_i16(*ptr->ch++);
		if (len != 0) ret = ret << 4;
	}
	return ret;
}

static int8_t
Unpacker_int4 (unpacker_t* ptr)
{
	int8_t ret = to_i16(*ptr->ch++);

	if (8 <= ret) {
		return ret - (1 << 4);
	} else {
		return ret;
	}
}

static int8_t
Unpacker_int8 (unpacker_t* ptr)
{
	uint8_t ret = to_i16all(ptr, 2);
	union {
		uint8_t uint8;
		int8_t int8;
	} cb = {ret};
	return cb.int8;
}

static int16_t
Unpacker_int16 (unpacker_t* ptr)
{
	uint16_t ret = to_i16all(ptr, 4);
	union {
		uint16_t uint16;
		int16_t int16;
	} cb = {ret};
	return cb.int16;
}

static int32_t
Unpacker_int32 (unpacker_t* ptr)
{
	uint32_t ret = to_i16all(ptr, 8);
	union {
		uint32_t uint32;
		int32_t int32;
	} cb = {ret};
	return cb.int32;
}

static uint64_t
Unpacker_int64 (unpacker_t* ptr)
{
	uint64_t ret = to_i16all(ptr, 16);
	union {
		uint64_t uint64;
		int64_t int64;
	} cb = {ret};
	return cb.int64;
}

static uint64_t
Unpacker_uint (unpacker_t* ptr, size_t len)
{
	return to_i16all(ptr, len);
}

static double
Unpacker_float (unpacker_t* ptr, size_t len)
{
	uint64_t ret = to_i16all(ptr, len);
	/* for convert unsigned long <-> float */
	union {
		unsigned long u;
		double f64;
	} converter = {ret};
	return converter.f64;
}

static VALUE
Unpacker_str (unpacker_t* ptr, size_t len)
{
	char* head = ptr->ch;

	VALUE str = rb_str_new(head, len);
	ptr->ch += len;
	rb_funcall(str, rb_intern("force_encoding"), 1, rb_str_new2("utf-8"));
	return str;
}

static VALUE
Unpacker_map (unpacker_t* ptr, size_t len)
{
	VALUE map = rb_hash_new();
	VALUE key, value;
	while (len--) {
		key = Unpacker_buffer_read(ptr);
		value = Unpacker_buffer_read(ptr);
		rb_hash_aset(map, key, value);
	}
	return map;
}

static VALUE
Unpacker_array (unpacker_t* ptr, size_t len)
{
	VALUE array = rb_ary_new2(len);
	while (len--) {
		rb_ary_push(array, Unpacker_buffer_read(ptr));
	}
	return array;
}

static VALUE
Unpacker_buffer_read (unpacker_t* ptr)
{
	uint64_t num;

	switch (*ptr->ch++) {
		case 'a': // int 4
			num = Unpacker_int4(ptr);
			return INT2FIX(num);

		case 'b': // int 8
			num = Unpacker_int8(ptr);
			return INT2FIX(num);

		case 'c': // int 16
			num = Unpacker_int16(ptr);
			return INT2FIX(num);

		case 'd': // int 32
			num = Unpacker_int32(ptr);
			return LONG2NUM(num);

		case 'e': // int 64
			num = Unpacker_int64(ptr);
			return rb_ll2inum(num);

		case 'g': // uint 8
			num = Unpacker_uint(ptr, 2);
			return INT2FIX(num);

		case 'h': // uint 16
			num = Unpacker_uint(ptr, 4);
			return INT2FIX(num);

		case 'i': // uint 32
			num = Unpacker_uint(ptr, 8);
			return LONG2NUM(num);

		case 'j': // uint 64
			num = Unpacker_uint(ptr, 16);
			return rb_ull2inum(num);

		case 'k': // float 32
			return rb_float_new(Unpacker_float(ptr, 8));

		case 'l': // float 64
			return rb_float_new(Unpacker_float(ptr, 16));

		case 'n': // str 8
			num = Unpacker_uint(ptr, 2);
			return Unpacker_str(ptr, num);

		case 'o': // str 16
			num = Unpacker_uint(ptr, 4);
			return Unpacker_str(ptr, num);

		case 'p': // str 32
			num = Unpacker_uint(ptr, 8);
			return Unpacker_str(ptr, num);

		case 'r': // map 4
			num = Unpacker_uint(ptr, 1);
			return Unpacker_map(ptr, num);

		case 's': // map 8
			num = Unpacker_uint(ptr, 2);
			return Unpacker_map(ptr, num);

		case 't': // map16
			num = Unpacker_uint(ptr, 4);
			return Unpacker_map(ptr, num);

		case 'u': // map 32
			num = Unpacker_uint(ptr, 8);
			return Unpacker_map(ptr, num);

		case 'v': // array 4
			num = Unpacker_uint(ptr, 1);
			return Unpacker_array(ptr, num);

		case 'w': // array 8
			num = Unpacker_uint(ptr, 2);
			return Unpacker_array(ptr, num);

		case 'x': // array 16
			num = Unpacker_uint(ptr, 4);
			return Unpacker_array(ptr, num);

		case 'y': // array 32
			num = Unpacker_uint(ptr, 8);
			return Unpacker_array(ptr, num);

		// positive fixint
		case '0': return INT2FIX(0);
		case '1': return INT2FIX(1);
		case '2': return INT2FIX(2);
		case '3': return INT2FIX(3);
		case '4': return INT2FIX(4);
		case '5': return INT2FIX(5);
		case '6': return INT2FIX(6);
		case '7': return INT2FIX(7);
		case '8': return INT2FIX(8);
		case '9': return INT2FIX(9);
		case 'A': return INT2FIX(10);
		case 'B': return INT2FIX(11);
		case 'C': return INT2FIX(12);
		case 'D': return INT2FIX(13);
		case 'E': return INT2FIX(14);
		case 'F': return INT2FIX(15);

		// fixstr
		case 'G':
			return rb_str_new2("");
		case 'H':
		case 'I':
		case 'J':
		case 'K':
		case 'L':
		case 'M':
		case 'N':
		case 'O':
		case 'P':
		case 'Q':
		case 'R':
		case 'S':
		case 'T':
		case 'U':
		case 'V':
			num = *(ptr->ch - 1) - 'G';
			return Unpacker_str(ptr, num);

		// others
		case 'W': return Qnil;
		case 'X': return Qfalse;
		case 'Y': return Qtrue;
	}

	rb_raise(rb_eArgError, "undefined type:%c,data:%s,at:%ld", *(ptr->ch), ptr->buffer, ptr->ch - ptr->buffer);
	return Qnil;
}

static VALUE
Unpacker_read (VALUE self)
{
	UNPACKER(self, ptr);

	return Unpacker_buffer_read(ptr);
}

static void
Unpacker_buffer_feed (unpacker_t* ptr, VALUE obj)
{
	char* p;

	if (rb_type(obj) != T_STRING) {
		rb_raise(rb_eArgError, "can not unpack object");
	}

	p = RSTRING_PTR(obj);

	ptr->buffer = p;
	ptr->ch = p;
}

static VALUE
Unpacker_feed (VALUE self, VALUE obj)
{
	UNPACKER(self, ptr);

	Unpacker_buffer_feed(ptr, obj);

	return self;
}

static void
Unpacker_buffer_clear (unpacker_t* ptr)
{
	ptr->buffer = NULL;
	ptr->ch = NULL;
}

static VALUE
Unpacker_clear (VALUE self)
{
	UNPACKER(self, ptr);

	Unpacker_buffer_clear(ptr);

	return self;
}

static VALUE
AsciiPack_unpack (int argc, VALUE *argv, VALUE self)
{
	VALUE res;
	VALUE unpacker = rb_funcall(cAsciiPack_Unpacker, rb_intern("new"), 0);

	rb_funcall(unpacker, rb_intern("feed"), 1, argv[0]);
	res = rb_funcall(unpacker, rb_intern("read"), 0);
	return res;
}

void
AsciiPack_Unpacker_init(VALUE mAsciiPack)
{
	cAsciiPack_Unpacker = rb_define_class_under(mAsciiPack, "Unpacker", rb_cObject);

	rb_define_alloc_func(cAsciiPack_Unpacker, Unpacker_alloc);

	rb_define_method(cAsciiPack_Unpacker, "initialize", Unpacker_initialize, -1);
	rb_define_method(cAsciiPack_Unpacker, "feed", Unpacker_feed, 1);
	rb_define_method(cAsciiPack_Unpacker, "read", Unpacker_read, 0);
	rb_define_alias(cAsciiPack_Unpacker, "<<", "feed");
	rb_define_method(cAsciiPack_Unpacker, "clear", Unpacker_clear, 0);

	rb_define_module_function(mAsciiPack, "unpack", AsciiPack_unpack, -1);
}
