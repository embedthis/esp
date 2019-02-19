assist - Build Assistance
===

These files "assist" when building with MakeMe, Gulp and working with GitLab.

* build-parts           - Invoke "me", "make" or "gulp" to build parts of an app
* cdrc                  - Copy to .cdrc to define environment for interactive use
* common                - Included by other scripts
* configure             - Core logic for top level configure script
* configure-node        - Configuration for node apps
* deploy-parts          - Invoke "me", "make" or "gulp" to deploy parts of an app
* docker-login          - Functions to login to the AWS ECR docker repository
* docker-publish        - Publish an image to the AWS ECR docker repository
* gitlab-app.yml        - Gitlab rules for a "gulp" based app
* gitlab-makeme.yml     - Gitlab rules for MakeMe projects
* gitlab-website.yml    - Gitlab rules for web site projects
* index.js              - Module to read configuration for gulp projects
* json2env              - Convert a JSON config file to environment key=value commands
* publish-parts         - Invoke "me", "make" or "gulp" to publish parts of an app
* remote-access         - Included script to configure remote access via SSH to AWS
* s3-publish-files      - Copy file tree to S3

## Secrets

Secrets are needed when building and executing applications. This development pattern stores secrets encrypted in S3 and in the application repository.

#### S3 Secrets

- Keys stored encrypted in S3 for use by Gitlab for CI/CD. The secrets are stored in buckets named {PROFILE}.config.
- The "secrets" script is used to get/set secrets.
- Gitlab build scripts use these secrets to access AWS accounts.
- The gitlab repository defines the secrets in the "keys" directory and includes a MakeMe script to define in S3.
- gitlab/keys/* has the actual AWS ops account keys to use by gitlab
- In the future, we need script to update these keys with the output from terraform commands.

```
eval $(paks/assist/secrets --env '' --profile "${PROFILE}" --aws-profile "${AWS_PROFILE}" get)
```
