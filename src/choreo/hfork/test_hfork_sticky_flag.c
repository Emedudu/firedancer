#include "fd_hfork.c"

#define SCRATCH_MAX (1UL<<18)
static uchar scratch[ SCRATCH_MAX ] __attribute__((aligned(128)));

static bhm_t *
query_bhm( fd_hfork_t * hfork, fd_hash_t const * block_id, fd_hash_t const * bank_hash ) {
  bhm_key_t key = { .block_id = *block_id, .bank_hash = *bank_hash };
  return bhm_map_ele_query( hfork->bhm_map, &key, NULL, hfork->bhm_pool );
}

int
main( int argc, char ** argv ) {
  fd_boot( &argc, &argv );

  fd_hfork_t * hfork = fd_hfork_join( fd_hfork_new( scratch, 4UL, 2UL, 0UL ) );
  FD_TEST( hfork );

  fd_pubkey_t voters[2] = { { .ul = { 1UL } }, { .ul = { 2UL } } };
  fd_hfork_update_voters( hfork, voters, 2UL );

  fd_hash_t block_id   = { .ul = { 100UL } };
  fd_hash_t their_hash = { .ul = { 200UL } };
  fd_hash_t our_hash   = { .ul = { 201UL } };

  FD_TEST( FD_HFORK_SUCCESS==fd_hfork_count_vote( hfork, &voters[0], &block_id, &their_hash, 10UL, 60UL, 100UL ) );
  FD_TEST( FD_HFORK_SUCCESS==fd_hfork_count_vote( hfork, &voters[1], &block_id, &their_hash, 11UL,  1UL, 100UL ) );

  FD_TEST( FD_HFORK_ERR_MISMATCHED==fd_hfork_record_our_bank_hash( hfork, &block_id, &our_hash, 100UL ) );
  bhm_t * bhm = query_bhm( hfork, &block_id, &their_hash );
  FD_TEST( bhm && bhm->stake==61UL );
  FD_LOG_NOTICE(( "before voter removal: mismatch stake is %lu", bhm->stake ));

  /* Remove the 60-stake voter.  The remaining bank-hash support is now
     1%, but blk->flag remains sticky and check() returns it directly. */
  fd_hfork_update_voters( hfork, &voters[1], 1UL );
  bhm = query_bhm( hfork, &block_id, &their_hash );
  FD_TEST( bhm && bhm->stake==1UL );
  FD_LOG_NOTICE(( "after voter removal: mismatch stake is %lu", bhm->stake ));

  FD_TEST( FD_HFORK_ERR_MISMATCHED==fd_hfork_record_our_bank_hash( hfork, &block_id, &our_hash, 100UL ) );
  FD_LOG_NOTICE(( "stale mismatch flag remains set below threshold" ));

  fd_hfork_delete( fd_hfork_leave( hfork ) );
  FD_LOG_NOTICE(( "pass" ));
  fd_halt();
  return 0;
}
