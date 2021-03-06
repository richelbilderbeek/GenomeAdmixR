#' Plot the frequencies of all ancestors over time
#' @description This function plots the frequency of all ancestors over time at
#' a specific location on the chromosome, after performing a simulation.
#' @param frequencies  A tibble containing four columns: \code{time},
#' \code{location}, \code{ancestor}, \code{frequency}. A fifth colum
#' \code{population} can be included if the tibble is the result of
#' \code{simulate_admixture_migration}.
#' @param focal_location Location (in Morgan) where to plot the allele
#' frequencies.
#' @return a ggplot2 object
#' @examples
#' pop <- simulate_admixture(pop_size = 1000,
#'                           number_of_founders = 10,
#'                           total_runtime = 11,
#'                           morgan = 1,
#'                           markers = 0.5,
#'                           seed = 42)
#' plot_over_time(pop$frequencies,
#'                focal_location = 0.5)
#' @export
plot_over_time <- function(frequencies,
                           focal_location) {

  p1 <- c()
  to_plot <- subset(frequencies, frequencies$location == focal_location)
  if (length(to_plot$time) == 0) {
    stop("No data to plot, are you sure that the focal location\nis within the
        vector of molecular markers?\n")
  }

  if ("population" %in% colnames(frequencies)) {
    p1 <- ggplot2::ggplot(to_plot,
                          ggplot2::aes(x = to_plot$time,
                                       y = to_plot$frequency,
                                       col = interaction(to_plot$ancestor,
                                                         to_plot$population))) +
      ggplot2::geom_step()
  } else {
    p1 <- ggplot2::ggplot(to_plot,
                          ggplot2::aes(x = to_plot$time,
                                       y = to_plot$frequency,
                                       col = as.factor(to_plot$ancestor))) +
      ggplot2::geom_step()
  }

  p1 <- p1 +
    ggplot2::xlab("Time (Generations") +
    ggplot2::ylab("Frequency") +
    ggplot2::labs(col = "Ancestor")

  return(p1)
}
