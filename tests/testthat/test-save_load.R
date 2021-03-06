context("create_populations")

test_that("save_population", {
  pop_size <- 100
  number_of_founders <- 10
  run_time <- 10
  morgan <- 1

  vx <- simulate_admixture(pop_size = pop_size,
                             number_of_founders = number_of_founders,
                             total_runtime = run_time,
                             morgan = morgan,
                             seed = 42)

  testthat::expect_true(verify_population(vx))

  save_population(vx, file_name = "test.pop")

  vy <- load_population(file_name = "test.pop")

  testthat::expect_true(verify_population(vy$population))

  testthat::expect_equal(length(vx$population), length(vy$population))

  for (i in seq_along(vx)) {
    testthat::expect_true(all.equal(vx[[i]], vy[[i]]))
  }
})
