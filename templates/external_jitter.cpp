#include "c74_min.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>

class __CLASS_NAME__ : public c74::min::object<__CLASS_NAME__>, public c74::min::matrix_operator<> {
public:
	MIN_DESCRIPTION{"__DESCRIPTION__"};
	MIN_TAGS{"__TAGS__"};
	MIN_AUTHOR{"__AUTHOR__"};

	c74::min::outlet<> matrix_out{this, "(jit_matrix) output matrix", "jit_matrix"};

	// === Sink (process incoming matrix) ===
	// calc_cell is called for every pixel. Capture full frame at (0,0).
	template <class matrix_type, size_t plane_count>
	c74::min::cell<matrix_type, plane_count> calc_cell(
		c74::min::cell<matrix_type, plane_count> input,
		const c74::min::matrix_info& info,
		c74::min::matrix_coord& position)
	{
		if constexpr(plane_count == 4) {
			if(position.x() == 0 && position.y() == 0) {
				auto size = info.width() * info.height() * info.cellsize();
				m_frame_buffer.resize(size);
				std::memcpy(m_frame_buffer.data(), info.m_bip, size);
			}
			return input;
		}
		return input;
	}

	// === Generator (output matrix from external data) ===
	// Uncomment and replace m_decoded_* with your data source.
	// IMPORTANT: Do NOT lock a mutex inside calc_cell per-pixel — it kills
	// real-time performance. Instead, swap the buffer at position(0,0) only.
	//
	// template <class matrix_type, size_t plane_count>
	// c74::min::cell<matrix_type, plane_count> calc_cell(
	//     c74::min::cell<matrix_type, plane_count> input,
	//     const c74::min::matrix_info& info,
	//     c74::min::matrix_coord& position)
	// {
	//     if constexpr(plane_count == 4) {
	//         if(!m_has_frame.load(std::memory_order_acquire)) {
	//             return {0, 0, 0, 255};
	//         }
	//         long x = position.x();
	//         long y = position.y();
	//         if(x == 0 && y == 0) {
	//             std::lock_guard<std::mutex> lock(m_frame_mutex);
	//             m_render_frame.swap(m_decoded_frame);
	//         }
	//         if(x >= m_decoded_width || y >= m_decoded_height) {
	//             return {0, 0, 0, 255};
	//         }
	//         std::size_t offset = (y * m_decoded_width + x) * 4;
	//         return {
	//             static_cast<matrix_type>(m_render_frame[offset + 0]),
	//             static_cast<matrix_type>(m_render_frame[offset + 1]),
	//             static_cast<matrix_type>(m_render_frame[offset + 2]),
	//             static_cast<matrix_type>(m_render_frame[offset + 3])
	//         };
	//     }
	//     return input;
	// }

private:
	std::vector<uint8_t> m_frame_buffer;

	// Generator pattern member variables (uncomment when needed):
	// std::vector<uint8_t> m_decoded_frame;
	// std::vector<uint8_t> m_render_frame;
	// long m_decoded_width = 0;
	// long m_decoded_height = 0;
	// std::mutex m_frame_mutex;
	// std::atomic<bool> m_has_frame{false};

	// Thread-safe message passing (uncomment when handling worker thread callbacks):
	// c74::min::queue<> m_queue{this, MIN_FUNCTION {
	//     return {};
	// }};
};

MIN_EXTERNAL(__CLASS_NAME__);
