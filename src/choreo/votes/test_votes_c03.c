#include "fd_votes.c"

#define SCRATCH_MAX (1UL<<20)
static uchar scratch[ SCRATCH_MAX ] __attribute__((aligned(128)));

static fd_votes_blk_t *
query_blk( fd_votes_t * votes, ulong slot, fd_hash_t const * block_id ) {
  fd_votes_blk_key_t key = { .slot = slot, .block_id = *block_id };
  return blk_map_ele_query( votes->blk_map, &key, NULL, votes->blk_pool );
}

int
main( int argc, char ** argv ) {
  fd_boot( &argc, &argv );

  fd_votes_t * votes = fd_votes_join( fd_votes_new( scratch, 16UL, 4UL, 0UL ) );
  FD_TEST( votes );

  fd_pubkey_t old_voter = { .ul = { 1UL } };
  fd_pubkey_t new_voter = { .ul = { 2UL } };
  fd_hash_t   block_id  = { .ul = { 100UL } };
  ulong       old_stake = 70UL;

  fd_votes_update_voters( votes, &old_voter, &old_stake, 1UL );
  fd_votes_publish( votes, 100UL );

  FD_TEST( FD_VOTES_SUCCESS==fd_votes_count_vote( votes, &old_voter, 105UL, &block_id ) );
  fd_votes_blk_t * blk = query_blk( votes, 105UL, &block_id );
  FD_TEST( blk && blk->stake==70UL );
  FD_LOG_NOTICE(( "before epoch update: block stake is %lu", blk->stake ));

  /* Epoch update removes the old voter.  The slot bit is cleared, but
     the per-block aggregate stake is not recomputed. */
  ulong new_stake = 1UL;
  fd_votes_update_voters( votes, &new_voter, &new_stake, 1UL );

  blk = query_blk( votes, 105UL, &block_id );
  FD_TEST( blk && blk->stake==70UL );
  FD_LOG_NOTICE(( "after removing voter: stale block stake is still %lu", blk->stake ));

  /* The freed voter bit can be reused, so the new voter is counted on
     top of stale stake from a voter no longer in the active set. */
  FD_TEST( FD_VOTES_SUCCESS==fd_votes_count_vote( votes, &new_voter, 105UL, &block_id ) );
  blk = query_blk( votes, 105UL, &block_id );
  FD_TEST( blk && blk->stake==71UL );
  FD_LOG_NOTICE(( "after new voter reuses bit: block stake is %lu", blk->stake ));

  fd_votes_delete( fd_votes_leave( votes ) );
  FD_LOG_NOTICE(( "pass" ));
  fd_halt();
  return 0;
}
