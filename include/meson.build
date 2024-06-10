inc_include = [include_directories('.')]

conf_data = configuration_data()
conf_data.set('version',version)
configure_file(input: 'config.h.in',
        output: 'config.h',
        configuration: conf_data)