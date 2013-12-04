import matplotlib.pyplot as plt
import pandas.tools.rplot as rplot
import pandas
import argh
import functools
import itertools
import logging

from driver import COLUMNS


def plotter(figure_name):
    """
    Simple decorator that wraps a function to clear the current plotting
    window, run the function, save the current plotting window, and finally
    clear the given window.
    """
    def outer(f):
        @functools.wraps(f)
        def wrapper(*args, **kwargs):
            plt.clf()
            f(*args, **kwargs)
            plt.savefig(figure_name + ".pdf")
            plt.clf()
        return wrapper
    return outer

TRELLIS = (
    (
        "max_depth_trellis",
        lambda df: trellis_plot(df[df.depth == max(df.depth)])
    ),
    (
        "min_depth_trellis",
        lambda df: trellis_plot(df[df.depth == min(df.depth)])
    ),
    (
        "all_trellis",
        lambda df: trellis_plot(df[df.depth % 2 == 0])
    ),
)


def slice_perms():
    dimensions = {'num_trees', 'num_features', 'depth'}
    operations = [max, min]
    for (xaxis, operation) in itertools.product(dimensions, operations):
        (f1, f2) = dimensions - {xaxis}
        constraints = \
            lambda df: (df[f1] == operation(df[f1])) & (df[f2] == operation(df[f2]))

        name = "{} at {} {}, {}".format(xaxis, operation.__name__, f1, f2)
        yield (name, lambda df: line_plot(df[constraints(df)], xaxis))

SLICES = tuple(s for s in slice_perms())


def plots(filename, run_trellis=False, run_slices=False):
    df = pandas.read_csv(filename, names=COLUMNS)
    # Group by everything except time_ns

    # Compute minima and drop the duplicate elements
    df['min_time_ns'] = df.groupby(list(COLUMNS)[:-1]).transform(min)
    del df['time_ns']
    df = df.drop_duplicates()

    if run_slices:
        for name, f in SLICES:
            plotter(name)(f)(df)

    if run_trellis:
        for name, f in TRELLIS:
            plotter(name)(f)(df)


def line_plot(df, xaxis):
    logging.info("Slice plot of \n%s", df.describe())
    for key, group in df.groupby('algorithm'):
        plt.plot(group[xaxis], group['min_time_ns'], label=key)
    plt.xlabel(xaxis)
    plt.ylabel('Time (ns)')
    plt.legend(loc='best')


def trellis_plot(df):
    logging.info("Trellis plot of \n%s", df.describe())
    plot = rplot.RPlot(df, x='num_trees', y='min_time_ns')
    plot.add(rplot.TrellisGrid(['num_features', 'depth']))
    plot.add(rplot.GeomPoint(
        colour=rplot.ScaleRandomColour('algorithm'),
        alpha=0.8,
        size=50))
    plot.render(plt.gcf())


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    argh.dispatch_command(plots)
