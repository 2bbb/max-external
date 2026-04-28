#include "c74_min.h"

class __CLASS_NAME__ : public c74::min::object<__CLASS_NAME__>, public c74::min::matrix_operator<> {
public:
	MIN_DESCRIPTION{"__DESCRIPTION__"};
	MIN_TAGS{"__TAGS__"};
	MIN_AUTHOR{"__AUTHOR__"};

	// For sink (processing): inlet receives matrix, outlet sends messages
	// For generator (output): no inlet needed, first outlet = jit_matrix, second = messages
	c74::min::outlet<> matrix_out{this, "(jit_matrix) output matrix", "jit_matrix"};

	// Jitter MOP attributes example:
	// c74::min::attribute<int> width{this, "width", 640,
	//     c74::min::description{"Matrix width"},
	//     c74::min::range{16, 3840}
	// };

	// --- Sink (process incoming matrix) ---
	// calc_cell is called for every pixel. Capture full frame at (0,0).
	template <class matrix_type, size_t plane_count>
	c74::min::cell<matrix_type, plane_count> calc_cell(
		c74::min::cell<matrix_type, plane_count> input,
		const c74::min::matrix_info& info,
		c74::min::matrix_coord& position)
	{
		if constexpr(plane_count == 4) {
			// RGBA processing
			// For full-frame capture, do it only at (0,0):
			// if(position.x() == 0 && position.y() == 0) {
			//     std::memcpy(buffer, info.m_bip, info.width() * info.height() * 4);
			// }
			// return input;
			return input;
		}
		return input;
	}

	// --- Generator (output decoded/received matrix) ---
	// Uncomment for generator mode:
	// template <class matrix_type, size_t plane_count>
	// c74::min::cell<matrix_type, plane_count> calc_cell(
	//     c74::min::cell<matrix_type, plane_count> input,
	//     const c74::min::matrix_info& info,
	//     c74::min::matrix_coord& position)
	// {
	//     if constexpr(plane_count == 4) {
	//         long x = position.x();
	//         long y = position.y();
	//         if(x >= frame_width || y >= frame_height) {
	//             return {0, 0, 0, 255};
	//         }
	//         std::size_t offset = (y * frame_width + x) * 4;
	//         return {
	//             static_cast<matrix_type>(frame[offset + 0]),
	//             static_cast<matrix_type>(frame[offset + 1]),
	//             static_cast<matrix_type>(frame[offset + 2]),
	//             static_cast<matrix_type>(frame[offset + 3])
	//         };
	//     }
	//     return input;
	// }

	// --- Thread-safe message passing ---
	// Worker threads must use queue<> to send outlet messages:
	// c74::min::queue<> m_queue{this, MIN_FUNCTION {
	//     deliver_pending_messages();
	//     return {};
	// }};
};

MIN_EXTERNAL(__CLASS_NAME__);
