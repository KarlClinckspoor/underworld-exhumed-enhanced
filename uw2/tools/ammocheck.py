"""Checks the enhancement auto-ammo (docs/ENHANCEMENTS.md): on entering a square the player takes
the missiles lying there that merge with a stack already carried (ui/INTERACT.C's auto_ammo,
called from motion/PHYSICS.C's square change; UW1's code, with UW2's pressure plates), unless they
would take the player over the weight they can carry.

What it changes is the inventory and the floor, so the port's ammo probe (UW2PORT_AMMO_PROBE, port
only) reports them on stderr each time the player enters a square: 'ammo-probe: square X Y
carried N floor F weight W', the missiles carried (the inventory and the containers in it), those
resting on that square, and the weight carried. The probe changes nothing, and reports with the
enhancement off as with it on.

Two setup sessions (port-made, with a test's cheats) write the saved games: tests/replay/setup/
auto-ammo, a new character with 10 sling stones in the pack, standing at (19, 51) on level 1 (two
squares south of the castle's door) with 5 more at their feet; and auto-ammo-heavy, the same
carrying all they can (strength 10, so 430, and the weight carried set to 430). The sessions
tests/replay/enhanced/auto-ammo and auto-ammo-heavy load them, walk forward off the square and
back onto it again. Each is replayed with the probe as it was recorded (with the enhancement), and
again with its enhancements chunk naming skip-intro alone (the original).

    python3 tools/ammocheck.py

Passes when every replay ends by itself, leaves the square and comes back to it, and on coming
back: auto-ammo with the enhancement carries 15 and leaves none on the floor, the weight carried
up by the 5 stones; without it carries 10 with 5 on the floor; auto-ammo-heavy carries 10 with 5
on the floor and weight 430, with the enhancement as without it. Exit status 0 when every check
passes."""
import os, re, sys, struct, shutil, tempfile
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here)
import replay as R

SESSIONS = os.path.join(os.path.dirname(here), 'tests', 'replay')
PROBE = 'UW2PORT_AMMO_PROBE'
FLAG = 'auto-ammo'
SQUARE = (19, 51)                       # where the 5 stones lie
CARRIED, DROPPED = 10, 5
HEAVY_WEIGHT = 430                      # all the heavy setup's player can carry


def names_of(data):
    """The names in a recording's enhancements chunk, or None if it has none. In formats 5 and 6
    the chunk follows the 12-byte header: stream 9, a 16-bit length, the names separated by
    commas (as replay.py's dos_can_replay reads it)."""
    if len(data) < 15 or data[4] not in (5, 6) or data[12] != 9: return None
    n = struct.unpack_from('<H', data, 13)[0]
    return data[15:15 + n].decode('ascii').split(',')


def without_flag(data):
    """The recording with FLAG taken out of its enhancements chunk, or None if it has none."""
    names = names_of(data)
    if names is None or FLAG not in names: return None
    n = struct.unpack_from('<H', data, 13)[0]
    kept = ','.join(x for x in names if x != FLAG).encode()
    return data[:12] + b'\x09' + struct.pack('<H', len(kept)) + kept + data[15 + n:]


def probed(log):
    """[((x, y), carried, floor, weight)] from the probe's lines, one a square entered."""
    return [((int(m[1]), int(m[2])), int(m[3]), int(m[4]), int(m[5]))
            for m in re.finditer(r'ammo-probe: square (\d+) (\d+) carried (\d+) floor (\d+) weight (\d+)', log)]


def run_port(rec, out, stage=None, probe=False):
    """R.run_port, with the probe on or off (UW2's run_port takes the port's environment from
    this process's)."""
    if probe: os.environ[PROBE] = '1'
    else: os.environ.pop(PROBE, None)
    try:
        return R.run_port(rec, out, [], stage=stage, quiet=True)
    finally:
        os.environ.pop(PROBE, None)


def main():
    bad, done = [], []
    def check(what, ok, detail=''):
        done.append(what)
        print(('ok    ' if ok else 'FAIL  ') + what + ('' if ok or not detail else f'\n      {detail}'))
        if not ok: bad.append(what)
    work = tempfile.mkdtemp(prefix='ammocheck-')
    runs = {}
    try:
        for name in ('auto-ammo', 'auto-ammo-heavy'):
            setup = os.path.join(work, name, 'setup')
            rc = run_port(os.path.join(SESSIONS, 'setup', name, 'session.rec'), setup)
            save = os.path.join(setup, 'SAVE1')
            check(f'setup/{name} replayed, writing its saved game (slot 1)', rc == 0 and os.path.isdir(save), f'status {rc}')
            if bad: return 1
            stage = os.path.join(work, name, 'stage')
            shutil.copytree(save, os.path.join(stage, 'SAVE1'))
            session = os.path.join(SESSIONS, 'enhanced', name, 'session.rec')
            data = open(session, 'rb').read()
            off = without_flag(data)
            check(f'{name}: the session carries {FLAG}, and a copy without it can be made', off is not None and FLAG not in names_of(off))
            if bad: return 1
            offrec = os.path.join(work, name, 'off.rec'); open(offrec, 'wb').write(off)
            shutil.copy(os.path.splitext(session)[0] + '.cfg', os.path.splitext(offrec)[0] + '.cfg')  # its settings, which replay reads from beside it
            for tag, rec in (('with the enhancement', session), ('without it', offrec)):
                out = os.path.join(work, name, tag.replace(' ', '-'))
                rc = run_port(rec, out, stage, probe=True)
                log = open(os.path.join(out, 'port.log'), errors='replace').read()
                check(f'{name} {tag}: replayed to its end (status 0)', rc == 0, f'status {rc}: ' + '\n      '.join(log.splitlines()[-6:]))
                p = probed(log)
                runs[name, tag] = p
                check(f'{name} {tag}: left {SQUARE} and came back to it',
                      len(p) >= 2 and p[-1][0] == SQUARE and any(q[0] != SQUARE for q in p[:-1]), p)
            if name == 'auto-ammo':
                rc = run_port(session, os.path.join(work, name, 'unprobed'), stage)
                log = open(os.path.join(work, name, 'unprobed', 'port.log'), errors='replace').read()
                check('the probe reports nothing when off', rc == 0 and 'ammo-probe' not in log)
        last = {k: v[-1] if v else None for k, v in runs.items()}
        on, off = last['auto-ammo', 'with the enhancement'], last['auto-ammo', 'without it']
        away = runs['auto-ammo', 'without it'][0] if runs['auto-ammo', 'without it'] else None
        check(f'auto-ammo: before coming back, {CARRIED} carried', away is not None and away[1] == CARRIED, away)
        check(f'auto-ammo with the enhancement: the {DROPPED} stones taken ({CARRIED + DROPPED} carried, none on the floor)',
              on is not None and on[1:3] == (CARRIED + DROPPED, 0), on)
        check(f'auto-ammo without it (the original): {CARRIED} carried, {DROPPED} on the floor',
              off is not None and off[1:3] == (CARRIED, DROPPED), off)
        check('auto-ammo: the weight carried up by the stones taken, and only with the enhancement',
              on is not None and off is not None and away is not None and off[3] == away[3] and on[3] > off[3], (away, on, off))
        for tag in ('with the enhancement', 'without it'):
            h = last['auto-ammo-heavy', tag]
            check(f'auto-ammo-heavy {tag}: too heavy, so {CARRIED} carried, {DROPPED} on the floor, weight {HEAVY_WEIGHT}',
                  h is not None and h[1:] == (CARRIED, DROPPED, HEAVY_WEIGHT), h)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print(f'ammocheck: {len(done) - len(bad)} of {len(done)} checks pass')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
