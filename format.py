#!/usr/bin/env python3

"""This script invokes format.py in the formulaslug/styleguide repository.

Set the FSAE_FORMAT environment variable to its location on disk before use. For
example:

FSAE_FORMAT="$HOME/styleguide" ./format.py
"""

import os
import subprocess
import sys

def main():
    path = os.environ.get("FSAE_FORMAT")
    if path == None:
        print("Error: FSAE_FORMAT environment variable not set")
        sys.exit(1)

    # Run main format.py script
    args = ["python", path + "/format.py"]
    args.extend(sys.argv[1:])
    proc = subprocess.Popen(args)
    proc.wait()

if __name__ == "__main__":
    main()
