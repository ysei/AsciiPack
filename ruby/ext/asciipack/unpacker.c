#include "unpacker.h"

VALUE cAsciiPack_Unpacker;

static VALUE Unpacker_read(unpacker_t* ptr);

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

static void
move (unpacker_t* ptr)
{
	ptr->ch++;
}

static char*
cut (unpacker_t* ptr, size_t n)
{
	char* ret = malloc(sizeof(char) * n);
	memset(ret, 0, n);
	memcpy(ret, ptr->ch, n);
	ptr->ch += n;
	return ret;
}

static VALUE
Unpacker_init(VALUE self, VALUE obj, int argc, VALUE *argv, VALUE size)
{
	UNPACKER(self, ptr);

	if (!ptr) {
		rb_raise(rb_eArgError, "unallocated unpacker");
	}

	ptr->buffer = RSTRING_PTR(obj);
	ptr->ch = ptr->buffer;

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
		ret += to_i16(*ptr->ch);
		ptr->ch++;
		if (len != 0) ret = ret << 4;
	}
	return ret;
}

static uint64_t
Unpacker_int (unpacker_t* ptr, size_t len)
{
	size_t n = len;
	char* head = ptr->ch;
	uint64_t base = 1;
	uint64_t ret = to_i16all(ptr, len);

	if ('8' <= *head) {
		ret -= (base << (n * 4));
	}
	return ret;
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
	union udouble converter;

	converter.u = ret;
	return converter.f64;
}

static VALUE
Unpacker_map (unpacker_t* ptr, size_t len)
{
	VALUE map = rb_hash_new();
	while (len--) {
		rb_hash_aset(map, Unpacker_read(ptr), Unpacker_read(ptr));
	}
	return map;
}

static VALUE
Unpacker_array (unpacker_t* ptr, size_t len)
{
	VALUE array = rb_ary_new2(len);
	while (len--) {
		rb_ary_push(array, Unpacker_read(ptr));
	}
	return array;
}

static VALUE
Unpacker_read (unpacker_t* ptr)
{
	uint64_t num;

	ptr->ch++;

	switch (*(ptr->ch - 1)) {
		case 'a': // int 4
			num = Unpacker_int(ptr, 1);
			return INT2FIX(num);
		case 'b': // int 8
			num = Unpacker_int(ptr, 2);
			return INT2FIX(num);
		case 'c': // int 16
			num = Unpacker_int(ptr, 4);
			return INT2FIX(num);
		case 'd': // int 32
			num = Unpacker_int(ptr, 8);
			return LONG2NUM(num);
		case 'e': // int 64
			num = Unpacker_int(ptr, 16);
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
		case 'n':
//			return rb_funcall(self, rb_intern("str8"), 0);
		case 'o':
//			return rb_funcall(self, rb_intern("str16"), 0);
		case 'p':
//			return rb_funcall(self, rb_intern("str32"), 0);
		case 'r':
			num = Unpacker_uint(ptr, 1);
			return Unpacker_map(ptr, num);
		case 's':
			num = Unpacker_uint(ptr, 2);
			return Unpacker_map(ptr, num);
		case 't':
			num = Unpacker_uint(ptr, 4);
			return Unpacker_map(ptr, num);
		case 'u':
			num = Unpacker_uint(ptr, 8);
			return Unpacker_map(ptr, num);
		case 'v':
			num = Unpacker_uint(ptr, 1);
			return Unpacker_array(ptr, num);
		case 'w':
			num = Unpacker_uint(ptr, 2);
			return Unpacker_array(ptr, num);
		case 'x':
			num = Unpacker_uint(ptr, 4);
			return Unpacker_array(ptr, num);
		case 'y':
			num = Unpacker_uint(ptr, 8);
			return Unpacker_array(ptr, num);
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
		case 'G':
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
	//		return rb_funcall(self, rb_intern("fixstr"), 0);
		case 'W': return Qnil;
		case 'X': return Qfalse;
		case 'Y': return Qtrue;
	}
	rb_raise(rb_eArgError, "undefined type:%c", *(ptr->ch));
	return Qnil;
}

static VALUE
Unpacker_unpack (VALUE self)
{
	UNPACKER(self, ptr);

	return Unpacker_read(ptr);
}

void
Init_asciipack(void)
{
// TODO
//	rb_define_method(rb_cArray, "to_asciipack", obj_to_asciipack, -1);
//	rb_define_method(rb_cHash,  "to_asciipack", obj_to_asciipack, -1);

	VALUE mAsciiPack = rb_path2class("AsciiPack");
	VALUE cAsciiPack_Unpacker = rb_define_class_under(mAsciiPack, "Unpacker", rb_cObject);

	rb_define_alloc_func(cAsciiPack_Unpacker, Unpacker_alloc);

	rb_define_method(cAsciiPack_Unpacker, "unpack", Unpacker_unpack, 0);
	rb_define_method(cAsciiPack_Unpacker, "initialize", Unpacker_initialize, -1);
}
