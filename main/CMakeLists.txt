idf_component_register(SRCS "main.cpp" "Timer.cpp" "Switch.cpp"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES "certs/cacert.pem"
                                   "certs/prvtkey.pem")
