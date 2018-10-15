


__global__ void gasal_semi_global_kernel(uint32_t *packed_query_batch, uint32_t *packed_target_batch,  uint32_t *query_batch_lens, uint32_t *target_batch_lens, uint32_t *query_batch_offsets, uint32_t *target_batch_offsets, int32_t *score, int32_t *target_batch_end, int n_tasks) {
	int32_t i, j, k, l, m;
	int32_t e;
	int32_t maxHH =  MINUS_INF;//initialize the maximum score to -infinity
	int32_t subScore;
	int32_t ridx, gidx;
	short2 HD;
	short2 initHD = make_short2(0, 0);
	int32_t maxXY_y = 0;
	const uint32_t tid = (blockIdx.x * blockDim.x) + threadIdx.x;//thread ID
	if (tid >= n_tasks) return;
	uint32_t packed_target_batch_idx = target_batch_offsets[tid] >> 3;//starting index of the target_batch sequence
	uint32_t packed_query_batch_idx = query_batch_offsets[tid] >> 3;//starting index of the query_batch sequence
	uint32_t read_len = query_batch_lens[tid];
	uint32_t ref_len = target_batch_lens[tid];
	uint32_t query_batch_regs = (read_len >> 3) + (read_len&7 ? 1 : 0);//number of 32-bit words holding sequence of query_batch
	uint32_t target_batch_regs = (ref_len >> 3) + (ref_len&7 ? 1 : 0);//number of 32-bit words holding sequence of target_batch
	//-------arrays to save intermediate values----------------
	short2 global[MAX_SEQ_LEN];
	int32_t h[9];
	int32_t f[9];
	int32_t p[9];
	//----------------------------------------------------------

	global[0] = make_short2(0, MINUS_INF);
	for (i = 1; i < MAX_SEQ_LEN; i++) {
		global[i] = make_short2(-(_cudaGapO + (_cudaGapExtend*(i))), MINUS_INF);
	}

	for (i = 0; i < target_batch_regs; i++) { //target_batch sequence in rows
		gidx = i << 3;
		ridx = 0;
		for (m = 0; m < 9; m++) {
			h[m] = 0;
			f[m] = MINUS_INF;
			p[m] = 0;
		}

		register uint32_t gpac =packed_target_batch[packed_target_batch_idx + i];//load 8 packed bases from target_batch sequence

		for (j = 0; j < query_batch_regs; j+=1) { //query_batch sequence in columns
			register uint32_t rpac =packed_query_batch[packed_query_batch_idx + j];//load 8 packed bases from query_batch sequence

			//--------------compute a tile of 8x8 cells-------------------
			for (k = 28; k >= 0; k -= 4) {
				uint32_t rbase = (rpac >> k) & 15;//get a base from query_batch sequence
				//------------load intermediate values----------------------
				HD = global[ridx];
				h[0] = HD.x;
				e = HD.y;
				//----------------------------------------------------------
				//int32_t prev_hm_diff = h[0] - _cudaGapOE;
	#pragma unroll 8
				for (l = 28, m = 1; m < 9; l -= 4, m++) {
					uint32_t gbase = (gpac >> l) & 15;//get a base from target_batch sequence
					DEV_GET_SUB_SCORE_GLOBAL(subScore, rbase, gbase);//check the equality of rbase and gbase
					//int32_t curr_hm_diff = h[m] - _cudaGapOE;
					f[m] = max(h[m]- _cudaGapOE, f[m] - _cudaGapExtend);//whether to introduce or extend a gap in query_batch sequence
					h[m] = p[m] + subScore;//score if gbase is aligned to rbase
					h[m] = max(h[m], f[m]);
					e = max(h[m - 1] - _cudaGapOE, e - _cudaGapExtend);//whether to introduce or extend a gap in target_batch sequence
					//prev_hm_diff=curr_hm_diff;
					h[m] = max(h[m], e);
					p[m] = h[m-1];
				}
				//--------------save intermediate values-------------------------
				HD.x = h[m-1];
				HD.y = e;
				global[ridx] = HD;
				//---------------------------------------------------------------
				ridx++;
				//------the last column of DP matrix------------
				if (ridx == read_len) {
					//----find the maximum and the corresponding end position-----------
					for (m = 1; m < 9; m++) {
						maxXY_y = (h[m] > maxHH && (gidx + m -1) < ref_len) ? gidx + (m-1) : maxXY_y;
						maxHH = (h[m] > maxHH && (gidx + m -1) < ref_len) ? h[m] : maxHH;
					}
					//--------------------------------------------------------------------
				}
				//------------------------------------------------
			}
			//-------------------------------------------------------------

		}


	}
	score[tid] = maxHH;//copy the max score to the output array in the GPU mem
	target_batch_end[tid] =  maxXY_y;//copy the end position on the target_batch sequence to the output array in the GPU mem

	return;


}

__global__ void gasal_semi_global_with_start_kernel(uint32_t *packed_query_batch, uint32_t *packed_target_batch, uint32_t *query_batch_lens, uint32_t *target_batch_lens, uint32_t *query_batch_offsets, uint32_t *target_batch_offsets, int32_t *score, int32_t *target_batch_end, int32_t *target_batch_start, int n_tasks) {

	int32_t i, j, k, l, m;
	int32_t e;
	int32_t maxHH =  MINUS_INF;//initialize the maximum score to -infinity
	int32_t subScore;
	int32_t ridx, gidx;
	short2 HD;
	short2 initHD = make_short2(0, 0);
	int32_t maxXY_y = 0;
	const uint32_t tid = (blockIdx.x * blockDim.x) + threadIdx.x;//thread ID
	if (tid >= n_tasks) return;
	uint32_t packed_target_batch_idx = target_batch_offsets[tid] >> 3;//starting index of the target_batch sequence
	uint32_t packed_query_batch_idx = query_batch_offsets[tid] >> 3;//starting index of the query_batch sequence
	uint32_t read_len = query_batch_lens[tid];
	uint32_t ref_len = target_batch_lens[tid];
	uint32_t query_batch_regs = (read_len >> 3) + (read_len&7 ? 1 : 0);//number of 32-bit words holding sequence of query_batch
	uint32_t target_batch_regs = (ref_len >> 3) + (ref_len&7 ? 1 : 0);//number of 32-bit words holding sequence of target_batch
	//-------arrays to save intermediate values----------------
	short2 global[MAX_SEQ_LEN];
	int32_t h[9];
	int32_t f[9];
	int32_t p[9];
	//-------------------------------------------------------

	global[0] = make_short2(0, MINUS_INF);
	for (i = 1; i < MAX_SEQ_LEN; i++) {
		global[i] = make_short2(-(_cudaGapO + (_cudaGapExtend*(i))), MINUS_INF);
	}

	for (i = 0; i < target_batch_regs; i++) { //target_batch sequence in rows
		gidx = i << 3;
		ridx = 0;
		for (m = 0; m < 9; m++) {
			h[m] = 0;
			f[m] = MINUS_INF;
			p[m] = 0;
		}

		register uint32_t gpac =packed_target_batch[packed_target_batch_idx + i];//load 8 packed bases from target_batch sequence

		for (j = 0; j < query_batch_regs; /*++j*/ j+=1) { //query_batch sequence in columns
			register uint32_t rpac =packed_query_batch[packed_query_batch_idx + j];//load 8 packed bases from query_batch sequence

			//--------------compute a tile of 8x8 cells-------------------
			for (k = 28; k >= 0; k -= 4) {
				uint32_t rbase = (rpac >> k) & 15;//get a base from query_batch sequence
				//------------load intermediate values----------------------
				HD = global[ridx];
				h[0] = HD.x;
				e = HD.y;
				//----------------------------------------------------------
				//int32_t prev_hm_diff = h[0] - _cudaGapOE;
	#pragma unroll 8
				for (l = 28, m = 1; m < 9; l -= 4, m++) {

					uint32_t gbase = (gpac >> l) & 15;//get a base from target_batch sequence
					DEV_GET_SUB_SCORE_GLOBAL(subScore, rbase, gbase);//check the equality of rbase and gbase
					//int32_t curr_hm_diff = h[m] - _cudaGapOE;
					f[m] = max(h[m]- _cudaGapOE, f[m] - _cudaGapExtend);//whether to introduce or extend a gap in query_batch sequence
					h[m] = p[m] + subScore;//score if gbase is aligned to rbase
					h[m] = max(h[m], f[m]);
					e = max(h[m - 1] - _cudaGapOE, e - _cudaGapExtend);//whether to introduce or extend a gap in target_batch sequence
					//prev_hm_diff=curr_hm_diff;
					h[m] = max(h[m], e);
					p[m] = h[m-1];
				}
				//--------------save intermediate values-------------------------
				HD.x = h[m-1];
				HD.y = e;
				global[ridx] = HD;
				//---------------------------------------------------------------
				ridx++;
				//------the last column of DP matrix------------
				if (ridx == read_len) {
					//----find the maximum and the corresponding end position-----------
					for (m = 1; m < 9; m++) {
						maxXY_y = (h[m] > maxHH && (gidx + m -1) < ref_len) ? gidx + (m-1) : maxXY_y;
						maxHH = (h[m] > maxHH && (gidx + m -1) < ref_len) ? h[m] : maxHH;
					}
					//------------------------------------------------------------------
				}
				//-----------------------------------------------
			}
			//------------------------------------------------------------




		}

	}

	score[tid] = maxHH;//copy the max score to the output array in the GPU mem
	target_batch_end[tid] =  maxXY_y;//copy the end position on the target_batch sequence to the output array in the GPU mem


	/*------------------Now to find the start position-----------------------*/

	uint32_t reverse_query_batch[(MAX_SEQ_LEN>>3)];//array to hold the reverse query_batch sequence
	uint32_t reverse_target_batch[(MAX_SEQ_LEN>>3)];//array to hold the reverse query_batch sequence
	uint32_t reverse_query_batch_reg;
	uint32_t reverse_target_batch_reg;

	for (i = 0; i < (MAX_SEQ_LEN>>3); i++) {
		reverse_query_batch[i] = 0;
	}
	for (i = 0; i < (MAX_SEQ_LEN>>3); i++) {
		reverse_target_batch[i] = 0;
	}

	//--------reverse query_batch sequence--------------------
	for (i = read_len - 1, k = 0; i >= 0; i--, k++) {
		uint32_t orig_query_batch_reg = i >> 3;
		uint32_t orig_symbol_pos = (((orig_query_batch_reg + 1) << 3) - i) - 1;
		reverse_query_batch_reg = k >> 3;
		uint32_t reverse_symbol_pos = (((reverse_query_batch_reg + 1) << 3) - k) - 1;
		uint32_t orig_symbol = 0;
		orig_symbol = (packed_query_batch[packed_query_batch_idx + orig_query_batch_reg] >> (orig_symbol_pos << 2)) & 15;
		reverse_query_batch[reverse_query_batch_reg] |= (orig_symbol << (reverse_symbol_pos << 2));
	}
	//---------------------------------------------------


	//--------reverse query_batch sequence--------------------
	for (i = ref_len - 1, k = 0; i >= 0; i--, k++) {
		uint32_t orig_target_batch_reg = i >> 3;
		uint32_t orig_symbol_pos = (((orig_target_batch_reg + 1) << 3) - i) - 1;
		reverse_target_batch_reg = k >> 3;
		uint32_t reverse_symbol_pos = (((reverse_target_batch_reg + 1) << 3) - k) - 1;
		uint32_t orig_symbol = 0;
		orig_symbol = (packed_target_batch[packed_target_batch_idx + orig_target_batch_reg] >> (orig_symbol_pos << 2)) & 15;
		reverse_target_batch[reverse_target_batch_reg] |= (orig_symbol << (reverse_symbol_pos << 2));
	}
	//---------------------------------------------------

	int32_t gend_pos = maxXY_y;//end position on target_batch sequence
	int32_t fwd_score = maxHH;//the computed score

	//the index of 32-bit word containing the end position on target_batch sequence
	int32_t gend_reg = (target_batch_regs - ((gend_pos >> 3) + 1)) > 0 ? (target_batch_regs - ((gend_pos >> 3) + 1)) - 1 : (target_batch_regs - ((gend_pos >> 3) + 1));

	maxHH = MINUS_INF;
	maxXY_y = 0;

	global[0] = make_short2(0, MINUS_INF);
	for (i = 1; i < MAX_SEQ_LEN; i++) {
		global[i] = make_short2(-(_cudaGapO + (_cudaGapExtend*(i))), MINUS_INF);
	}

	//------starting from the gend_reg, align the sequences in the reverse direction and exit if the max score >= fwd_score------
	for (i = gend_reg; i < target_batch_regs && maxHH < fwd_score; i++) { //target_batch sequence in rows
		gidx = i << 3;
		ridx = 0;
		for (m = 0; m < 9; m++) {
			h[m] = 0;
			f[m] = MINUS_INF;
			p[m] = 0;
		}

		register uint32_t gpac =reverse_target_batch[i];//load 8 packed bases from target_batch sequence

		for (j = 0; j < query_batch_regs && maxHH < fwd_score;j+=1) { //query_batch sequence in columns
			register uint32_t rpac =reverse_query_batch[j];//load 8 packed bases from target_batch sequence
			//--------------compute a tile of 8x8 cells-------------------
			for (k = 28; k >= 0; k -= 4) {
				uint32_t rbase = (rpac >> k) & 15;//get a base from query_batch sequence
				//------------load intermediate values----------------------
				HD = global[ridx];
				h[0] = HD.x;
				e = HD.y;
				//--------------------------------------------------------
				//int32_t prev_hm_diff = h[0] - _cudaGapOE;
	#pragma unroll 8
				for (l = 28, m = 1; m < 9; l -= 4, m++) {
					uint32_t gbase = (gpac >> l) & 15;//get a base from target_batch sequence
					DEV_GET_SUB_SCORE_GLOBAL(subScore, rbase, gbase);//check the equality of rbase and gbase
					//int32_t curr_hm_diff = h[m] - _cudaGapOE;
					f[m] = max(h[m]- _cudaGapOE, f[m] - _cudaGapExtend);//whether to introduce or extend a gap in query_batch sequence
					h[m] = p[m] + subScore;//score if gbase is aligned to rbase
					h[m] = max(h[m], f[m]);
					e = max(h[m - 1] - _cudaGapOE, e - _cudaGapExtend);//whether to introduce or extend a gap in target_batch sequence
					//prev_hm_diff=curr_hm_diff;
					h[m] = max(h[m], e);
					p[m] = h[m-1];
				}
				//------------save intermediate values----------------------
				HD.x = h[m-1];
				HD.y = e;
				global[ridx] = HD;
				//----------------------------------------------------------
				ridx++;
				//------the last column of DP matrix------------
				if (ridx == read_len) {
					//----find the maximum and the corresponding end position-----------
					for (m = 1; m < 9; m++) {
						maxXY_y = (h[m] > maxHH && (gidx + (m -1)) < ref_len) ? gidx + (m-1) : maxXY_y;
						maxHH = (h[m] > maxHH && (gidx + (m -1)) < ref_len ) ? h[m] : maxHH;
					}
					//------------------------------------------------------------------
				}
				//----------------------------------------------
			}
			//---------------------------------------------------------------


		}

	}
	//-----------------------------------------------------------------------------------------------------------------

	target_batch_start[tid] = (ref_len - 1) - maxXY_y;//copy the start position on target_batch sequence to the output array in the GPU mem

	return;



}