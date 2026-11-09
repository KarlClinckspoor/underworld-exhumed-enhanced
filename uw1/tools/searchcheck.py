"""Checks that find_good_x_and_y's search reads only what it has cleared (docs/PORT.md, "The
search for room near a square"; docs/FINDINGS.md, "Finding room near a spot"). The original
declares its visited table, one bit a square of the search's box, as 9 words for the box's 10
columns and clears 9 of its 18 bytes, so the columns east of the start begin as whatever the
stack held: the port had its own native stack's leftovers there, which could make a teleport
skip the free square beside its destination, or find none. Those bytes are stack junk, cleared
now in the replay build and the port (STACK_JUNK_SET); the word past the table, which DOS's
frame shares with next_list's first entry, the port keeps where DOS has it.

The way in is the game's own: the setup session tests/replay/setup/plain-debris (port-made)
writes a saved game of a new character on level 1, and the session tests/replay/search
(port-made from its script, with a test's cheat, format 6) loads it and teleports the player
onto (43,9), a diagonal tile whose wall runs through the middle of the square. The search then
tries the squares the diagonal opens onto: (44,9), east, in a column the original leaves
uncleared, where the player fits, and (43,8), rock. Before the fix the port refused this
teleport ("no free square there").

The session is replayed with the port's search probe (UW1PORT_SEARCH_PROBE: a line on stderr
for each search, where it started and what it found), once as it is and once with the visited
table filled with set bits before it is cleared (UW1PORT_SEARCH_POISON), as the worst stack
would leave it: with the whole table cleared the poison changes nothing.

    python3 tools/searchcheck.py [--config PATH]

Passes when the replays end by themselves, the teleport's search from (43,9) finds (44,9) and
the arrival's search from there finds (44,9) again, the same with the poison, and the probe
says nothing when off. Exit status 0 when every check passes."""
import os, re, sys, shutil, tempfile
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here)
from exhume import EXHUME
sys.path.insert(0, os.path.join(EXHUME, 'tools'))
if '--config' not in sys.argv: sys.argv[1:1] = ['--config', os.path.join(os.path.dirname(here), 'exhume.toml')]
import replay as R

SETUP = os.path.join(R.RC.sessions, 'setup', 'plain-debris', 'session.rec')
SESSION = os.path.join(R.RC.sessions, 'search', 'session.rec')
PROBE, POISON = 'UW1PORT_SEARCH_PROBE', 'UW1PORT_SEARCH_POISON'
START, FREE = (43, 9), (44, 9)
EXPECT = [(START, FREE), (FREE, FREE)]  # the teleport's search, then the arrival's


def searches(log):
    """[((x, y), (nx, ny) or None)] from the probe's lines (the searches that clear nothing)."""
    out = []
    for m in re.finditer(r'search-probe: from (\d+) (\d+) clear (\d) (?:found (\d+) (\d+)|none)', log):
        if m[3] == '0':
            out.append(((int(m[1]), int(m[2])), (int(m[4]), int(m[5])) if m[4] else None))
    return out


def run_port(rec, out, stage=None, env=None):
    return R.run_port(rec, out, [], stage=stage, quiet=True, env=env)


def main():
    bad = []
    def check(what, ok, detail=''):
        print(('ok    ' if ok else 'FAIL  ') + what + ('' if ok or not detail else f'\n      {detail}'))
        if not ok: bad.append(what)
    work = tempfile.mkdtemp(prefix='searchcheck-')
    try:
        setup = os.path.join(work, 'setup')
        rc = run_port(SETUP, setup)
        save = os.path.join(setup, 'SAVE1')
        check('setup/plain-debris replayed, writing its saved game', rc == 0 and os.path.isdir(save), f'status {rc}')
        if bad: return 1
        stage = os.path.join(work, 'stage')
        shutil.copytree(save, os.path.join(stage, 'SAVE1'))
        runs = {}
        for tag, env in (('as it is', {PROBE: '1', POISON: None}),
                         ('with the table poisoned', {PROBE: '1', POISON: '1'}),
                         ('with the probe off', {PROBE: None, POISON: None})):
            out = os.path.join(work, tag.replace(' ', '-'))
            rc = run_port(SESSION, out, stage, env)
            log = open(os.path.join(out, 'port.log'), errors='replace').read()
            check(f'search, {tag}: replayed to its end (status 0)', rc == 0 and 'host fault' not in log,
                  f'status {rc}: ' + '\n      '.join(log.splitlines()[-6:]))
            runs[tag] = searches(log)
        check(f'the teleport onto {START} finds {FREE}, the free square east of it, and the arrival stays there',
              runs['as it is'] == EXPECT, runs['as it is'])
        check('the same with the visited table filled with set bits first (the search clears all of it)',
              runs['with the table poisoned'] == EXPECT, runs['with the table poisoned'])
        check('the probe reports nothing when off', runs['with the probe off'] == [], runs['with the probe off'])
    finally:
        shutil.rmtree(work, ignore_errors=True)
    n = 7
    print(f'searchcheck: {n - len(bad)} of {n} checks pass')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
