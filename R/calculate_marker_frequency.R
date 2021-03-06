#' Calculate allele frequencies at a specific marker location
#' @description  Calculate the relative frequency of each ancestor in the
#' population at a specific marker location
#' @param pop Population for which to estimate allele frequencies at the
#' given marker
#' @param location A vector or scalar of location(s) along the chromosome for
#' which allele frequencies are to be calculated. Locations are in Morgan.
#' @return A tibble containing the frequency of each present ancestor at the
#' provided location. Ancestors with frequency = 0 are dropped out of the table.
#' The tibble contains three columns: location, ancestor and frequency.
#' @examples
#' \dontrun{
#' number_founders = 20
#' wildpop =  simulate_admixture(pop_size = 1000,
#'                               number_of_founders = number_founders,
#'                               total_runtime = 10,
#'                               morgan = 1,
#'                               seed = 666)
#'
#' calculate_marker_frequency(wildpop, 0.5)
#'
#' frequencies <- calculate_marker_frequency(wildpop, seq(0.4, 0.5, by = 0.001))
#'
#' ggplot(frequencies, aes(x = location, y = frequency, col = ancestor)) +
#'   geom_step()
#'
#' }
#' @export
calculate_marker_frequency <- function(pop, location) {

  pop <- check_input_pop(pop)

  per_loc <- function(loc) {
    fun_chrom <- function(indiv) {
      return(c(findtype(indiv$chromosome1, loc),
               findtype(indiv$chromosome2, loc)))
    }
    types <- unlist(lapply(pop, fun_chrom))

    vv <- tibble::as_tibble(table(types))
    colnames(vv) <- c("ancestor", "frequency")

    vv$frequency <- vv$frequency / sum(vv$frequency)
    vv$location <- loc
    return(vv)
  }

  all_types <- lapply(location, per_loc)
  output <- c()
  for (i in seq_along(all_types)) {
    output <- rbind(output, all_types[[i]])
  }
  output <- output[, c("location", "ancestor", "frequency")]

  return(output)
}
