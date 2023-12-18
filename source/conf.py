# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = "sc2-serializer"
copyright = "2023, Bryce Ferenczi, Rhys Newbury"
author = "Bryce Ferenczi, Rhys Newbury"

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = []

templates_path = ["_templates"]
exclude_patterns = []

# Add the path to the Doxygen XML output directory
doxygen_xml = "./docs/doxygen"
# Tell Sphinx to use the Doxygen XML
breathe_projects = {"myproject": doxygen_xml}
breathe_default_project = "myproject"

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = "alabaster"
html_static_path = ["_static"]
