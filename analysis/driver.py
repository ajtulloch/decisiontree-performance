from collections import namedtuple

import argh
import cStringIO
import itertools
import logging
import numpy as np
import os
import pandas
import random
import subprocess

DEVNULL = open(os.devnull, 'wb')
COLUMNS = ('algorithm', 'num_trees', 'depth', 'num_features', 'time_ns')

# Struct for all parameters for the decision trees binary
TreeArgs = namedtuple('TreeArgs', ['num_trees', 'depth', 'num_features'])

SWEEP = TreeArgs(
    num_trees=np.linspace(10, 1000, 10),
    depth=np.linspace(1, 6, 6),
    num_features=np.linspace(50, 10000, 5))


def run_command(args):
    logging.info("Running %s", args)
    cmd = [
        "./main", "--output_csv",
        "-num_trees", str(args.num_trees),
        "-depth", str(args.depth),
        "-num_features", str(args.num_features),
    ]
    logging.debug("Calling %s", cmd)
    return subprocess.check_output(cmd, stderr=DEVNULL)


def randomized(generator):
    """
    Returns a randomized permutation of the input generator.
    """
    combinations = list(generator)
    random.shuffle(combinations)
    for el in combinations:
        yield el


def raw_data_to_file(filename):
    """
    Sweeps over all parameter combinations, and outputs the generated
    CSV to the filename given as input.
    """
    buffer = cStringIO.StringIO()
    logging.info("Running parameter sweep %s", SWEEP)
    combinations = itertools.product(SWEEP.num_trees,
                                     SWEEP.depth,
                                     SWEEP.num_features)

    # Randomize the ordering of the combinations to help combat bias
    # in estimation.
    for combination in randomized(combinations):
        (num_trees, depth, num_features) = (int(el) for el in combination)
        buffer.write(run_command(TreeArgs(
            num_trees=num_trees,
            depth=depth,
            num_features=num_features)))

    with open(filename, "w") as f:
        f.write(buffer.getvalue())
    logging.info(pandas.read_csv(filename, names=COLUMNS).describe())


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    argh.dispatch_command(raw_data_to_file)
