To setup the project:

If you're not already in the folder 'wormhole', preform:

1. cd wormhole

Once you're there, set the variable LD_LIBRARY_PATH to the
current working directory using:

2. setenv LD_LIBRARY_PATH `pwd`

You can check (optionally) that this operation was exceuted properly using:

3. echo $LD_LIBRARY_PATH


Then, do:

4. cd sto

5. /./bootstrap.sh

6. ./configure

To run the test file do:

7. make unit-testMTrie

Then run it using:

8. ./unit-test_MTrie
