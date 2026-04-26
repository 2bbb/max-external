#include "c74_min.h"

class __CLASS_NAME__ : public c74::min::object<__CLASS_NAME__> {
public:
	MIN_DESCRIPTION{"__DESCRIPTION__"};
	MIN_TAGS{"__TAGS__"};
	MIN_AUTHOR{"__AUTHOR__"};

	c74::min::inlet<> input{this, "(anything) input"};
	c74::min::outlet<> output{this, "(anything) output"};

	c74::min::message<> bang_msg{this, "bang", "respond to bang",
		MIN_FUNCTION {
			output.send(c74::min::k_sym_bang);
			return {};
		}
	};
};

MIN_EXTERNAL(__CLASS_NAME__);
