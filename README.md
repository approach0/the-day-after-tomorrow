## About
This is a repo for a future math-aware search engine.

No worry, "the day after tomorrow" is a temporary name, will come up a more catchy name for this project someday. 

This project is originated from my previous [math-only search engine prototype](https://github.com/t-k-/opmes).
There are some amazing ideas coming up since then, so a decision is made to rewrite most the code.
This time, however, it has the ambition to become a production-level search engine with improved efficiency, and most importantly, to be able to handle queries with both math expressions and general text terms. In another word, to be "math-aware".

This project is still under early development so there is not really much to show here. 
But keep your eyes open and pull request is appreciated!

The meaning behind "the day after tomorrow"? Because reinventing a search engine in C has a long way to go, but as the saying goes:

> Today is hard, tomorrow will be worse, 
> but the day after tomorrow will be sunshine.
>
> (Jack Ma)

## Quick start

### 1. Install dependencies
Other than commonly system build-in libraries (pthread, libz, libm, libstdc++), ther are some external dependencies you may need to download and install to your system environment:

* [bison](http://ftp.gnu.org/gnu/bison/bison-3.0.tar.xz)
* [flex and libfl](http://sourceforge.net/projects/flex/files/flex-2.5.39.tar.xz/download)
* [libreadline](http://ftp.gnu.org/gnu/readline/readline-6.3.tar.gz)
* [libncurses](http://ftp.gnu.org/pub/gnu/ncurses/ncurses-5.7.tar.gz)
* [Lemur/Indri](https://sourceforge.net/projects/lemur/files/lemur/indri-5.9/indri-5.9.tar.gz/download)

For Debian/Ubuntu users, issue the following commands: 
```
$ sudo apt-get update
$ sudo apt-get install bison flex libreadline-dev libncurses5-dev
```
Lemur/Indri is not likely to be in your distribution's official software repository, so you may need to manually build it (see the next step).

Lemur/Indri library is an important dependency for this project, currently this project relies on it to provide full-text index functionality (so that we avoid reinventing the wheel, and we can focus on math search implementation. To combine two search engines, simply merge their results and weight their scores accordingly). 

 
After downloading Indri tarball (indri-5.9 for example), build its libraries:

```
$ (cd indri-5.9 && chmod +x configure && ./configure && make)
```

### 2. Configure dependency path
Our project uses `dep-*.mk` files (you will get an idea of what it is by just opening one of it) to configure dependency paths  (or CFLAGS and LDFLAGS). For system build-in libraries and downloaded libraries which you have just installed to your system environment, no need to specify their paths, leave these `dep-*.mk` files unchanged.

The only dependency you may need to specify manually upon building this project is the Lemur/Indri library: Given an example, if you download and compile Lemur/Indri at `~/indri-5.9`,  make sure there is a similar line `INDRI=/home/$(shell whoami)/indri-5.9` in file `$(PROJECT)/term-index/Makefile`.

### 3. Compile/build
Type `make` at project top level (i.e. `$(PROJECT)`) will do the job.

### 4. Test some commands you build
This project is still in its early stage, nothing really to show you now. However, you can play with some existing commands:

* Run our TeX parser to see the corresponding operator tree of a math expression:
	```
	$ ./tex-parser/run/test-tex-parser.out
	edit: a+b/c
	return code:SUCC
	Operator tree:
	    └──(plus) 2 son(s), token=ADD, path_id=1, ge_hash=0000, fr_hash=c301.
	        │──[`a'] token=VAR, path_id=1, ge_hash=c401, fr_hash=8703.
	        └──(frac) 2 son(s), token=FRAC, path_id=2, ge_hash=05f7, fr_hash=7203.
	               │──#1[`b'] token=VAR, path_id=2, ge_hash=c501, fr_hash=7403.
	               └──#2[`c'] token=VAR, path_id=3, ge_hash=c601, fr_hash=7503.

	Subpaths (leaf-root paths/total subpaths = 3/4):
	* VAR(0)/ADD(2)/[path_id=1: type=normal, leaf symbol=`a', fr_hash=8703]
	* VAR(0)/rank1(1)/FRAC(2)/ADD(2)/[path_id=2: type=normal, leaf symbol=`b', fr_hash=7403]
	* FRAC(2)/ADD(2)/[path_id=2: type=gener, ge_hash=05f7, fr_hash=7203]
	* VAR(0)/rank2(1)/FRAC(2)/ADD(2)/[path_id=3: type=normal, leaf symbol=`c', fr_hash=7503]
	```

* Index a corpus/collection and see its index statistics:
	1. Download some plain text corpus (e.g. *Reuters-21578* and *Ohsumed* from [University of Trento](http://disi.unitn.it/moschitti/corpora.htm)).
	2. `cd $(PROJECT)/indexer` and run `run/test-txt-indexer.out -p /path/to/corpus` to index corpus files recursively from directory. For non-trivial (reasonable large) corpus, you will have the chance to observe the index merging precess under default generated index directory (`$(PROJECT)/indexer/tmp`).
	3. `cd $(PROJECT)/indexer` and run `../term-index/run/test-read.out -s` to have a look at the summary of the index (termN, docN, avgDocLen etc.) you just build.

## Module dependency
![module dependency](https://raw.githubusercontent.com/t-k-/cowpie-lab/master/dep.png)
(boxes are external dependencies, circles are internal modules)

To generate this module dependency graph, issue commands below at the project top directory:

```
$ mkdir -p tmp
$ python3 proj-dep-graph.py > tmp/dep.dot
$ dot -Tpng tmp/dep.dot > tmp/dep.png
```

## License
MIT