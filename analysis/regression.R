library("nlme")
library("data.table")
library("memisc")
library("functional")
library("ggplot2")

kColumns <- c("algorithm", "num_trees", "depth", "num_features", "time_ns")
kFilterDepth <- 2
kFilterNumFeatures <- 5000
kFileWidth <- 13
kFileHeight <- 8
kFileExtension <- "png"

preprocess <- function(filename) {
  ## Could use read.table, but fails for some reason
  df <- data.table(read.csv(filename, col.names=kColumns))

  ## Get the minimum value from each experiment
  df <- df[,
           min_time_ns := min(time_ns), 
           by=list(algorithm, num_trees, depth, num_features)]
  ## Unique the rows so we just have the minimum time for each experiment. 
  setkey(df, NULL)
  df$time_ns <- NULL
  
  ## Round num_features to nearest 100 (prettier numbers)
  df$num_features <- 100 * round(df$num_features / 100)
  return(unique(df))
}

fit <- function(df) {
  fits <- lmList(min_time_ns ~ num_trees + depth + num_features | algorithm, 
                 data=df)
  return(fits)
}

analysis <- function(fits, to.latex=TRUE) {
  mt <- mtable("Naive"=fits$"Naive Forest", 
               "Piecewise Flattened"=fits$"Piecewise Flat Forest", 
               "Contiguous Flattened"=fits$"Contiguous Flat Forest",
               "Compiled"=fits$"Compiled Forest", 
               summary.stats=c("R-squared", "F", "p", "N"))
  
  if (to.latex) { toLatex(mt) } else { print(mt) }
}

plots <- function(df, directory, extension) {
  filter.depth <- kFilterDepth
  filter.num_features <- kFilterNumFeatures
  filter.title <- sprintf("Evaluation time against number of trees\nDepth: %s, Features: %s", 
                          filter.depth, filter.num_features)  
  filter.df <- subset(df, depth == filter.depth & num_features == filter.num_features)
  filter.plot <- Curry(plots.single,
                       title=filter.title,
                       extension=extension,
                       directory=directory)
  
  filter.plot(df=subset(filter.df),
              filename="all_snapshot")
  
  ## Trellis plots
  filter.trellis <- Curry(plots.trellis, 
                          title="Evaluation time against number of trees\nBroken down by number of features and depth",
                          directory=directory,
                          extension=extension)
  
  
  filter.trellis(df=df,
                 filename="all_trellis")
  filter.trellis(df=subset(df, num_features >= 2500 & num_features <= 7500 & depth <= 4 & depth >= 2),
                 filename="subset_trellis")
}

plots.single <- function(df, title, filename, directory, extension) {
  qplot(num_trees, 
        min_time_ns, 
        data=df, 
        geom=c("line", "point"), 
        color=algorithm, 
        main=title,
        alpha=I(.8))
  ggsave(file=sprintf("%s/%s.%s", directory, filename, extension), 
         width=kFileWidth, 
         height=kFileHeight)    
}

plots.trellis <- function(df, title, filename, directory, extension) {
  qplot(num_trees, 
        min_time_ns, 
        data=df, 
        geom=c("line", "point"), 
        color=algorithm, 
        facets=depth ~ num_features, 
        title=title,
        alpha=I(.8))
  ggsave(file=sprintf("%s/%s.%s", directory, filename, extension), 
         width=kFileWidth, 
         height=kFileHeight)
}


## Extract command line arguments
args <- commandArgs(trailingOnly = TRUE)
filename <- args[1]
directory <- args[2]

## Construct helper functions
regression <- Compose(preprocess, fit, Curry(analysis, to.latex=FALSE))
visualization <- Compose(preprocess, Curry(plots, directory=directory, extension=kFileExtension))

## Run the computations
regression(filename)
visualization(filename)

