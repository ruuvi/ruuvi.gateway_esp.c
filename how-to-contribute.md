# Contributing

When contributing to this repository, please first discuss the change you wish to make via issue
or [Ruuvi Slack](http://slack.ruuvi.com) #ruuvigateway channel. Open an issue describing what
you're going to do before starting work so others know that it is being worked on. 

## Pull Request Process

1. Once you have opened issue about the feature and @ojousima or @TheSomeMan has approved
it in principle, please fork the repository and work on your own fork.

2. If your work is a simple fix, there's no need for architectural discussion. If it is
a larger contribution, confirm beforehand where functionality should be. The best place might
be inside one of the components instead of in main project.
  - Pay special attention to memory safety, e.g. use `snprintf` instead of `sprintf`,
    check that pointer is not NULL before dereferencing it etc. 
  - Remember that this is embedded system, ensure that your loops have upper bound and
    memory gets freed after use.
  - Run `./scripts/run-clang-format.sh` to format your code.

3. Once the feature is ready for review, open a Pull Request. Code is automatically
checked by Jenkins: does it compile, do unit tests pass, does it follow the clang-format
definitions. If Jenkins rejects the pull request for any reason, check the Jenkins log
and fix any issues found.
  - If you are not on the list of allowlisted authors, Jenkins will ask admins to verify
    that your work is safe to run (e.g. no rm -rf / in makefiles). 
    - In that case one of the project admins will manually trigger the Jenkins build.

4. Once Jenkins build passes, request a review from @TheSomeMan. If this is your first time
contriburting to this repository, please leave a note saying BSD-3 Licensing is ok for you.
e.g. `I am the original author of this work, and I have the right to give the work to this 
project and agree to BSD-3 License`. 

5. Your work will be discussed for potential changes or fixes, and once the feature and
implementation are approved it will be merged to the project. Thanks for the contribution!

## Code of Conduct 

As contributors and maintainers of the project, we pledge to respect everyone who contributes by posting issues, updating documentation, submitting pull requests, providing feedback in comments, and any other activities.

Communication through any of Ruuvi's channels (GitHub, Slack, mailing lists, Twitter, etc.) must be constructive and never resort to personal attacks, trolling, public or private harassment, insults, or other unprofessional conduct.

We promise to extend courtesy and respect to everyone involved in this project regardless of gender, gender identity, sexual orientation, disability, age, race, ethnicity, religion, or level of experience. We expect anyone contributing to the Ruuvi project to do the same.

If any member of the community violates this code of conduct, the maintainers of the Ruuvi project may take action, removing issues, comments, and PRs or blocking accounts as deemed appropriate.

If you are subject to or witness unacceptable behavior, or have any other concerns, please email us at [contact@ruuvi.com](mailto:contact@ruuvi.com).

CoC is based on [Angular 0.3B](https://github.com/angular/code-of-conduct/blob/master/CODE_OF_CONDUCT.md)