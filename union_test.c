

struct bar {
	union {
		struct {
			int state_read_counter;
			int state_read_len;
		};
		struct {
			int state_write_counter;
			int state_write_len;
		};
	};
};


struct wow {
	struct bar;
};

