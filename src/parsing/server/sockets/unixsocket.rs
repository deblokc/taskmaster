use yaml_rust::yaml::{Hash, Yaml};

#[derive(Debug)]
pub struct UnixSocket {
    pub file: String,
    pub chmod: u16,
    pub user_chown: Option<String>,
    pub group_chown: Option<String>,
    pub username: Option<String>,
    pub password: Option<String>,
}

impl UnixSocket {
    pub fn create(server: &Hash) -> Result<Option<Self>, String> {
        match server.get(&Yaml::String(String::from("unixsocket"))) {
            None => Ok(None),
            Some(unixsocket) => match unixsocket.as_hash() {
                None => Err(format!("Unix socket block is not a map")),
                Some(param) => Ok(Some(UnixSocket {
                    file: UnixSocket::set_file(param)?,
                    chmod: UnixSocket::set_chmod(param)?,
                    user_chown: UnixSocket::set_user_chown(param)?,
                    group_chown: UnixSocket::set_group_chown(param)?,
                    username: UnixSocket::set_username(param)?,
                    password: UnixSocket::set_password(param)?,
                })),
            },
        }
    }

    fn set_file(param: &Hash) -> Result<String, String> {
        match param.get(&Yaml::String(String::from("file"))) {
            None => Ok(String::from("/tmp/taskmasterd.sock")),
            Some(file) => match file.as_str() {
                None => Err(format!(
                    "File parameter in Unix Socket block is not a valid string"
                )),
                Some(val) => Ok(String::from(val)),
            },
        }
    }

    fn set_chmod(param: &Hash) -> Result<u16, String> {
        match param.get(&Yaml::String(String::from("chmod"))) {
            None => Ok(0o0700),
            Some(chmod) => match chmod.as_i64() {
                None => Err(format!(
                    "chmod parameter in Unix Socket block is not a valid integer"
                )),
                Some(val) => {
                    if val > 0o7777 {
                        Err(format!("Invalid chmod parameter specified for unix socket, maximum value is 0o7777, found {val:#o}"))
                    } else if val < 0 {
                        Err(format!("Invalid chmod paramater specified for unix socket, negative value encountered"))
                    } else {
                        Ok(val as u16)
                    }
                }
            },
        }
    }
    fn set_user_chown(param: &Hash) -> Result<Option<String>, String> {
        match param.get(&Yaml::String(String::from("chown"))) {
            None => Ok(None),
            Some(chown) => match chown.as_str() {
                None => Err(format!(
                    "Chown parameter in Unix Socket block is not a valid string"
                )),
                Some(val) => {
                    let user = String::from(val);
                    if let Some(index) = user.find(':') {
                        if index == 0 {
                            Ok(None)
                        } else {
                            Ok(Some(user[..index].to_string()))
                        }
                    } else {
                        Ok(Some(user))
                    }
                }
            },
        }
    }

    fn set_group_chown(param: &Hash) -> Result<Option<String>, String> {
        match param.get(&Yaml::String(String::from("chown"))) {
            None => Ok(None),
            Some(chown) => match chown.as_str() {
                None => Err(format!(
                    "Chown parameter in Unix Socket block is not a valid string"
                )),
                Some(val) => {
                    let group = String::from(val);
                    if let Some(index) = group.find(':') {
                        if group.len() == index + 1 {
                            Ok(None)
                        } else {
                            Ok(Some(group[(index + 1)..].to_string()))
                        }
                    } else {
                        Ok(None)
                    }
                }
            },
        }
    }

    fn set_username(param: &Hash) -> Result<Option<String>, String> {
        match param.get(&Yaml::String(String::from("username"))) {
            None => Ok(None),
            Some(username) => match username.as_str() {
                None => Err(format!(
                    "Username parameter in Unix Socket block is not a valid string"
                )),
                Some(val) => Ok(Some(val.to_string())),
            },
        }
    }
    fn set_password(param: &Hash) -> Result<Option<String>, String> {
        match param.get(&Yaml::String(String::from("password"))) {
            None => Ok(None),
            Some(password) => match password.as_str() {
                None => Err(format!(
                    "Password parameter in Unix Socket block is not a valid string"
                )),
                Some(val) => Ok(Some(val.to_string())),
            },
        }
    }
}
