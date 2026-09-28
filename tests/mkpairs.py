"""Build tests/data/pairs.pgn, a synthetic engine tournament for GamePairs,
from real openings in parity.pgn.

    python tests/mkpairs.py tests/data/parity.pgn tests/data/pairs.pgn

Each pairing plays openings twice, once with each colour, and the pair
results cover every case (2-0, 1.5-0.5, 1-1 as two draws or as a win each,
0.5-1.5, 0-2). It also has the rough edges the batch tool meets in real
files: engine names where one is a prefix of another, openings played once or
three times, games that ended inside the opening, unfinished (*) games, and
"1-0" inside an Event tag (the batch's textReplace works on the whole file).
Seeded, so the same parity.pgn gives the same file."""
import random, re, sys

src, out = sys.argv[1], sys.argv[2]
text = open(src, encoding='latin-1').read().replace('\r\n', '\n')
games = [g for g in re.split(r'\n\n(?=\[Event )', text) if g.strip()]
rng = random.Random(23)

def moves_of(g):
    body = g.partition('\n\n')[2]
    toks = [t for t in body.split() if t not in ('1-0', '0-1', '1/2-1/2', '*')]
    return toks

def plies(toks):
    return [t for t in toks if not re.match(r'^\d+\.+$', t)]

# Openings: games with at least 30 plies, distinct first 16 plies.
seen, openings = set(), []
for g in games:
    t = moves_of(g)
    p = plies(t)
    if len(p) < 30:
        continue
    key = tuple(p[:16])
    if key in seen:
        continue
    seen.add(key)
    openings.append(t)
rng.shuffle(openings)

def game(event, rnd, white, black, result, toks):
    return ('[Event "%s"]\n[Site "Test"]\n[Date "2026.01.01"]\n[Round "%s"]\n'
            '[White "%s"]\n[Black "%s"]\n[Result "%s"]\n\n%s %s\n'
            % (event, rnd, white, black, result, ' '.join(toks), result))

def cut_game(toks, n_plies):
    """The first n_plies plies of a move list, with move numbers."""
    out, k = [], 0
    for t in toks:
        if not re.match(r'^\d+\.+$', t):
            if k == n_plies:
                break
            k += 1
        out.append(t)
    return out

engines = ['Alpha', 'Beta', 'Gamma 1', 'Gamma 10', 'Delta']
pairings = [('Alpha', 'Beta'), ('Alpha', 'Gamma 1'), ('Alpha', 'Gamma 10'),
            ('Alpha', 'Delta'), ('Beta', 'Gamma 1'), ('Gamma 10', 'Delta'),
            ('Beta', 'Delta')]
# Pair outcomes as (result of game 1 with A white, result of game 2 with B white).
outcomes = [('1-0', '0-1'), ('1-0', '1/2-1/2'), ('1/2-1/2', '0-1'),
            ('1/2-1/2', '1/2-1/2'), ('1-0', '1-0'), ('0-1', '0-1'),
            ('0-1', '1/2-1/2'), ('1/2-1/2', '1-0'), ('0-1', '1-0')]
out_games, oi, rnd = [], 0, 0
for a, b in pairings:
    for i in range(rng.randrange(8, 16)):
        o = openings[oi]; oi += 1
        r1, r2 = rng.choice(outcomes)
        ev = 'Match 1-0 series' if rng.random() < 0.1 else 'Test Gauntlet'
        rnd += 1
        out_games.append(game(ev, '%d.1' % rnd, a, b, r1, o))
        k = rng.random()
        if k < 0.05:
            continue                                   # opening played once
        out_games.append(game(ev, '%d.2' % rnd, b, a, r2, o))
        if k > 0.95:                                   # and a third time
            out_games.append(game(ev, '%d.3' % rnd, a, b, rng.choice(['1-0', '0-1', '1/2-1/2']), o))
    # a game that ended inside the opening, and an unfinished one
    o = openings[oi]; oi += 1
    out_games.append(game('Test Gauntlet', 'x%d' % rnd, a, b, '1-0', cut_game(o, 11)))
    o = openings[oi]; oi += 1
    out_games.append(game('Test Gauntlet', 'y%d' % rnd, b, a, '*', o))
rng.shuffle(out_games)
open(out, 'w', encoding='latin-1', newline='\n').write('\n'.join(out_games))
print(len(out_games), 'games,', oi, 'openings used')
