# ft_traceroute

### Prepare environment

```bash
docker build -t remote -f Dockerfile.remote .
docker run -d --cap-add sys_ptrace -p127.0.0.1:2222:22 --name remote_env remote
ssh-keygen -f "$HOME/.ssh/known_hosts" -R "[localhost]:2222"
```


### Usage

```zsh
./ft_traceroute google.com
```

```zsh

```

* https://habr.com/ru/post/281272/
* https://habr.com/ru/post/129627/